#include "opencv2/ximgproc/sparse_match_interpolator.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <queue>
#include <bitset>

#define INF 1E+20F

namespace cv {
namespace ximgproc {

SparseMatch::SparseMatch(Point2f ref_point, Point2f target_point)
{
    reference_image_pos = ref_point;
    target_image_pos = target_point;
}

bool operator<(const SparseMatch& lhs,const SparseMatch& rhs)
{
    if((int)(lhs.reference_image_pos.y+0.5f)!=(int)(rhs.reference_image_pos.y+0.5f))
        return (lhs.reference_image_pos.y<rhs.reference_image_pos.y);
    else
        return (lhs.reference_image_pos.x<rhs.reference_image_pos.x);
}

struct node
{
    float dist;
    short label;
    node() {}
    node(short l,float d): label(l), dist(d){}
};
void ransacInterpolationOld(Mat& labels, Mat& NNlabels, Mat& NNdistances, vector<SparseMatch>& matches, Mat& dst_dense_flow, int num_iter, vector<node>* g);

class EdgeAwareInterpolatorImpl : public EdgeAwareInterpolator
{
public:
    static Ptr<EdgeAwareInterpolatorImpl> create();
    void interpolate(InputArray reference_image, InputArray target_image, InputArray matches, OutputArray dense_flow);

    void setInlierEps(float eps);

protected:
    int w,h;
    int match_num;

    //internal data:
    vector<node>* g;
    Mat labels;
    Mat NNlabels;
    Mat NNdistances;

    //internal parameters:
    float lambda; // value between zero and one; controls edge sensitivity (default: 0.999f)
    int k;        // number of nearest-neighbor matches that we consider during interpolation (default: 128)
    float sigma;  // controls the speed of weight decay while increasing the distance (default: 0.05f)
    float inlier_eps; //threshold that defines inliers during RANSAC-based local affine transform fitting (default: 2.0f)
    float fgs_lambda; // default: 500.0f
    float fgs_sigma;  // default: 1.5f
    float regularization_coef;

    // aux parameters:
    int distance_transform_num_iter, ransac_interpolation_num_iter;
    static const int ransac_num_stripes = 4;
    RNG rngs[ransac_num_stripes];

    void init();
    void preprocessData(Mat& src, vector<SparseMatch>& matches);
    void ransacInterpolation(vector<SparseMatch>& matches, Mat& dst_dense_flow);

    void geodesicDistanceTransform(Mat& distances, Mat& cost_map);
    void buildGraph(Mat& distances, Mat& cost_map);
    void computeGradientMagnitude(Mat& src, Mat& dst);

protected:
    struct GetKNNMatches_ParBody : public ParallelLoopBody
    {
        EdgeAwareInterpolatorImpl* inst;
        int num_stripes;
        int stripe_sz;

        GetKNNMatches_ParBody(EdgeAwareInterpolatorImpl& _inst, int _num_stripes);
        void operator () (const Range& range) const;
    };

    struct RansacInterpolation_ParBody : public ParallelLoopBody
    {
        EdgeAwareInterpolatorImpl* inst;
        Mat* transforms;
        float* weighted_inlier_nums;
        SparseMatch* matches;
        int num_stripes;
        int stripe_sz;
        int inc;

        RansacInterpolation_ParBody(EdgeAwareInterpolatorImpl& _inst, Mat* _transforms, float* _weighted_inlier_nums, SparseMatch* _matches, int _num_stripes, int _inc);
        void operator () (const Range& range) const;
    };
};

void EdgeAwareInterpolatorImpl::setInlierEps(float eps)
{
    inlier_eps = eps;
}

void EdgeAwareInterpolatorImpl::init() 
{
    lambda     = 999.0f;
    k          = 128;
    sigma      = 0.05f;
    inlier_eps = 2.0f;
    fgs_lambda = 500.0f;
    fgs_sigma  = 1.5f;

    regularization_coef = 0.01f;
    distance_transform_num_iter   = 2;
    ransac_interpolation_num_iter = 1;
}

Ptr<EdgeAwareInterpolatorImpl> EdgeAwareInterpolatorImpl::create()
{
    EdgeAwareInterpolatorImpl *eai = new EdgeAwareInterpolatorImpl();
    eai->init();
    return Ptr<EdgeAwareInterpolatorImpl>(eai);
}

void EdgeAwareInterpolatorImpl::interpolate(InputArray reference_image, InputArray, InputArray matches, OutputArray dense_flow)
{
    w = reference_image.cols();
    h = reference_image.rows();
    
    vector<SparseMatch> matches_vector = *(const std::vector<SparseMatch>*)matches.getObj();
    std::sort(matches_vector.begin(),matches_vector.end());
    match_num = matches_vector.size();
    CV_Assert(match_num<SHRT_MAX);

    Mat src = reference_image.getMat();
    labels = Mat(h,w,CV_16S);
    labels = Scalar(-1);
    NNlabels = Mat(match_num,k,CV_16S);
    NNlabels = Scalar(-1);
    NNdistances = Mat(match_num,k,CV_32F);
    NNdistances = Scalar(0.0f);
    g = new vector<node>[match_num];
    preprocessData(src,matches_vector);

    dense_flow.create(reference_image.size(),CV_32FC2);
    Mat dst = dense_flow.getMat();
    ransacInterpolation(matches_vector,dst);
    fastGlobalSmootherFilter(src,dst,dst,fgs_lambda,fgs_sigma);
    delete[] g;
}

void EdgeAwareInterpolatorImpl::preprocessData(Mat& src, vector<SparseMatch>& matches)
{
    Mat distances(h,w,CV_32F);
    Mat cost_map (h,w,CV_32F);
    distances = Scalar(INF);

    int x,y;
    for(unsigned int i=0;i<matches.size();i++)
    {
        x = min((int)(matches[i].reference_image_pos.x+0.5f),w-1);
        y = min((int)(matches[i].reference_image_pos.y+0.5f),h-1);

        distances.at<float>(y,x) = 0.0f;
        labels.at<short>(y,x) = (short)i;
    }

    computeGradientMagnitude(src,cost_map);
    cost_map = (1000.0f-lambda) + lambda*cost_map;

    geodesicDistanceTransform(distances,cost_map);
    buildGraph(distances,cost_map);
    parallel_for_(Range(0,getNumThreads()),GetKNNMatches_ParBody(*this,getNumThreads()));
}

void EdgeAwareInterpolatorImpl::computeGradientMagnitude(Mat& src, Mat& dst)
{
    Mat dx,dy,src_gray;
    cvtColor(src,src_gray,COLOR_BGR2GRAY);
    Sobel(src_gray, dx, CV_16SC1, 1, 0);
    Sobel(src_gray, dy, CV_16SC1, 0, 1);
    float norm_coef = 4.0f*255.0f;

    for(int i=0;i<h;i++)
    {
        short* dx_row  = dx.ptr<short>(i);
        short* dy_row  = dy.ptr<short>(i);
        float* dst_row = dst.ptr<float>(i);

        for(int j=0;j<w;j++)
            dst_row[j] = ((float)abs(dx_row[j])+abs(dy_row[j]))/norm_coef;
    }
}

void EdgeAwareInterpolatorImpl::geodesicDistanceTransform(Mat& distances, Mat& cost_map)
{
    float c1 = 1.0f/2.0f;
    float c2 = sqrt(2.0f)/2.0f;
    float d = 0.0f;
    int i,j;
    float *dist_row,      *cost_row;
    float *dist_row_prev, *cost_row_prev;
    short *label_row;
    short *label_row_prev;

#define CHECK(cur_dist,cur_label,cur_cost,prev_dist,prev_label,prev_cost,coef)\
    d = prev_dist + coef*(cur_cost+prev_cost);\
    if(cur_dist>d){\
        cur_dist=d;\
        cur_label = prev_label;}

    for(int it=0;it<distance_transform_num_iter;it++)
    {
        //first pass (left-to-right, top-to-bottom):
        dist_row  = distances.ptr<float>(0);
        label_row = labels.ptr<short>(0);
        cost_row  = cost_map.ptr<float>(0);
        for(j=1;j<w;j++)
            CHECK(dist_row[j],label_row[j],cost_row[j],dist_row[j-1],label_row[j-1],cost_row[j-1],c1);

        for(i=1;i<h;i++)
        {
            dist_row       = distances.ptr<float>(i);
            dist_row_prev  = distances.ptr<float>(i-1);

            label_row      = labels.ptr<short>(i);
            label_row_prev = labels.ptr<short>(i-1);

            cost_row      = cost_map.ptr<float>(i);
            cost_row_prev = cost_map.ptr<float>(i-1);

            j=0;
            CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j]  ,label_row_prev[j]  ,cost_row_prev[j]  ,c1);
            CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j+1],label_row_prev[j+1],cost_row_prev[j+1],c2);            
            j++;
            for(;j<w-1;j++)
            {
                CHECK(dist_row[j],label_row[j],cost_row[j],dist_row[j-1]     ,label_row[j-1]     ,cost_row[j-1]     ,c1);
                CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j-1],label_row_prev[j-1],cost_row_prev[j-1],c2);
                CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j]  ,label_row_prev[j]  ,cost_row_prev[j]  ,c1);
                CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j+1],label_row_prev[j+1],cost_row_prev[j+1],c2);            
            }
            CHECK(dist_row[j],label_row[j],cost_row[j],dist_row[j-1]     ,label_row[j-1]     ,cost_row[j-1]     ,c1);
            CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j-1],label_row_prev[j-1],cost_row_prev[j-1],c2);
            CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j]  ,label_row_prev[j]  ,cost_row_prev[j]  ,c1);
        }

        //second pass (right-to-left, bottom-to-top):
        dist_row  = distances.ptr<float>(h-1);
        label_row = labels.ptr<short>(h-1);
        cost_row  = cost_map.ptr<float>(h-1);
        for(j=w-2;j>=0;j--)
            CHECK(dist_row[j],label_row[j],cost_row[j],dist_row[j+1],label_row[j+1],cost_row[j+1],c1);

        for(i=h-2;i>=0;i--)
        {
            dist_row       = distances.ptr<float>(i);
            dist_row_prev  = distances.ptr<float>(i+1);

            label_row      = labels.ptr<short>(i);
            label_row_prev = labels.ptr<short>(i+1);

            cost_row      = cost_map.ptr<float>(i);
            cost_row_prev = cost_map.ptr<float>(i+1);

            j=w-1;
            CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j]  ,label_row_prev[j]  ,cost_row_prev[j]  ,c1);
            CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j-1],label_row_prev[j-1],cost_row_prev[j-1],c2);            
            j--;
            for(;j>0;j--)
            {
                CHECK(dist_row[j],label_row[j],cost_row[j],dist_row[j+1]     ,label_row[j+1]     ,cost_row[j+1]     ,c1);
                CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j+1],label_row_prev[j+1],cost_row_prev[j+1],c2);
                CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j]  ,label_row_prev[j]  ,cost_row_prev[j]  ,c1);
                CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j-1],label_row_prev[j-1],cost_row_prev[j-1],c2);            
            }
            CHECK(dist_row[j],label_row[j],cost_row[j],dist_row[j+1]     ,label_row[j+1]     ,cost_row[j+1]     ,c1);
            CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j+1],label_row_prev[j+1],cost_row_prev[j+1],c2);
            CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j]  ,label_row_prev[j]  ,cost_row_prev[j]  ,c1);
        }
    }
#undef CHECK
}

void EdgeAwareInterpolatorImpl::buildGraph(Mat& distances, Mat& cost_map)
{
    float *dist_row,      *cost_row;
    float *dist_row_prev, *cost_row_prev;
    short *label_row;
    short *label_row_prev;
    int i,j,n;
    const float c1 = 1.0f/2.0f;
    const float c2 = sqrt(2.0f)/2.0f;
    float d;
    bool found;

#define CHECK(cur_dist,cur_label,cur_cost,prev_dist,prev_label,prev_cost,coef)\
    if(cur_label!=prev_label)\
    {\
        d = prev_dist + cur_dist + coef*(cur_cost+prev_cost);\
        found = false;\
        for(unsigned int n=0;n<g[prev_label].size();n++)\
        {\
            if(g[prev_label][n].label==cur_label)\
            {\
                g[prev_label][n].dist = min(g[prev_label][n].dist,d);\
                found=true;\
                break;\
            }\
        }\
        if(!found)\
            g[prev_label].push_back(node(cur_label ,d));\
    }

    dist_row  = distances.ptr<float>(0);
    label_row = labels.ptr<short>(0);
    cost_row  = cost_map.ptr<float>(0);
    for(j=1;j<w;j++)
        CHECK(dist_row[j],label_row[j],cost_row[j],dist_row[j-1],label_row[j-1],cost_row[j-1],c1);

    for(i=1;i<h;i++)
    {
        dist_row       = distances.ptr<float>(i);
        dist_row_prev  = distances.ptr<float>(i-1);

        label_row      = labels.ptr<short>(i);
        label_row_prev = labels.ptr<short>(i-1);

        cost_row      = cost_map.ptr<float>(i);
        cost_row_prev = cost_map.ptr<float>(i-1);

        j=0;
        CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j]  ,label_row_prev[j]  ,cost_row_prev[j]  ,c1);
        CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j+1],label_row_prev[j+1],cost_row_prev[j+1],c2);            
        j++;
        for(;j<w-1;j++)
        {
            CHECK(dist_row[j],label_row[j],cost_row[j],dist_row[j-1]     ,label_row[j-1]     ,cost_row[j-1]     ,c1);
            CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j-1],label_row_prev[j-1],cost_row_prev[j-1],c2);
            CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j]  ,label_row_prev[j]  ,cost_row_prev[j]  ,c1);
            CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j+1],label_row_prev[j+1],cost_row_prev[j+1],c2);            
        }
        CHECK(dist_row[j],label_row[j],cost_row[j],dist_row[j-1]     ,label_row[j-1]     ,cost_row[j-1]     ,c1);
        CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j-1],label_row_prev[j-1],cost_row_prev[j-1],c2);
        CHECK(dist_row[j],label_row[j],cost_row[j],dist_row_prev[j]  ,label_row_prev[j]  ,cost_row_prev[j]  ,c1);
    }
#undef CHECK

    // force equal distances in both directions:
    node* neighbors;
    for(i=0;i<match_num;i++)
    {
        neighbors = &g[i].front();
        for(j=0;j<(int)g[i].size();j++)
        {
            found = false;

            for(n=0;n<(int)g[neighbors[j].label].size();n++)
            {
                if(g[neighbors[j].label][n].label==i)
                {
                    neighbors[j].dist = g[neighbors[j].label][n].dist = min(neighbors[j].dist,g[neighbors[j].label][n].dist);
                    found = true;
                    break;
                }
            }

            if(!found)
                g[neighbors[j].label].push_back(node((short)i,neighbors[j].dist));
        }        
    }
}

struct nodeHeap
{
    // start indexing from 1 (root)
    // children: 2*i, 2*i+1
    // parent: i>>1
    node* heap;
    short* heap_pos;
    node tmp_node;
    short size;
    short num_labels;

    nodeHeap(short _num_labels)
    {
        num_labels = _num_labels;
        heap = new node[num_labels+1];
        heap[0] = node(-1,-1.0f);
        heap_pos = new short[num_labels];
        memset(heap_pos,0,sizeof(short)*num_labels);
        size=0;
    }

    ~nodeHeap()
    {
        delete[] heap;
        delete[] heap_pos;
    }

    void clear()
    {
        size=0;
        memset(heap_pos,0,sizeof(short)*num_labels);
    }

    inline bool empty()
    {
        return (size==0);
    }

    inline void nodeSwap(short idx1, short idx2)
    {
        heap_pos[heap[idx1].label] = idx2; 
        heap_pos[heap[idx2].label] = idx1;

        tmp_node   = heap[idx1];
        heap[idx1] = heap[idx2];
        heap[idx2] = tmp_node;
    }

    void add(node n)
    {
        size++;
        heap[size] = n;
        heap_pos[n.label] = size;
        short i = size;
        short parent_i = i>>1;
        while(heap[i].dist<heap[parent_i].dist)
        {
            nodeSwap(i,parent_i);
            i=parent_i;
            parent_i = i>>1;
        }
    }

    node getMin()
    {
        node res = heap[1];
        heap_pos[res.label] = 0;

        short i=1;
        short left,right;
        while( (left=i<<1) < size )
        {
            right = left+1;
            if(heap[left].dist<heap[right].dist)
            {
                heap[i] = heap[left];
                heap_pos[heap[i].label] = i;
                i = left;
            }
            else
            {
                heap[i] = heap[right];
                heap_pos[heap[i].label] = i;
                i = right;
            }        
        }

        if(i==size)
        {
            size--;
            return res;
        }

        heap[i] = heap[size];
        heap_pos[heap[i].label] = i;

        short parent_i = i>>1;
        while(heap[i].dist<heap[parent_i].dist)
        {
            nodeSwap(i,parent_i);
            i=parent_i;
            parent_i = i>>1;
        }

        size--;
        return res;
    }

    //checks if node is already in the heap
    //if not - add it
    //if it is - update it with the min dist of the two
    void updateNode(node n)
    {
        if(heap_pos[n.label])
        {
            short i = heap_pos[n.label];
            heap[i].dist = min(heap[i].dist,n.dist);
            short parent_i = i>>1;
            while(heap[i].dist<heap[parent_i].dist)
            {
                nodeSwap(i,parent_i);
                i=parent_i;
                parent_i = i>>1;
            }
        }
        else
            add(n);
    }
};

EdgeAwareInterpolatorImpl::GetKNNMatches_ParBody::GetKNNMatches_ParBody(EdgeAwareInterpolatorImpl& _inst, int _num_stripes):
inst(&_inst),num_stripes(_num_stripes)
{
    stripe_sz = (int)ceil(inst->match_num/(double)num_stripes);
}

void EdgeAwareInterpolatorImpl::GetKNNMatches_ParBody::operator() (const Range& range) const
{
    int start = std::min(range.start * stripe_sz, inst->match_num);
    int end   = std::min(range.end   * stripe_sz, inst->match_num);
    nodeHeap q((short)inst->match_num);
    int num_expanded_vertices;
    bitset<SHRT_MAX> expanded_flag;
    node* neighbors;

    for(int i=start;i<end;i++)
    {
        if(inst->g[i].empty())
            continue;
        
        num_expanded_vertices = 0;
        expanded_flag.reset();
        q.clear();
        q.add(node((short)i,0.0f));
        short* NNlabels_row    = inst->NNlabels.ptr<short>(i);
        float* NNdistances_row = inst->NNdistances.ptr<float>(i);
        while(num_expanded_vertices<inst->k && !q.empty())
        {
            node vert_for_expansion = q.getMin();
            expanded_flag[vert_for_expansion.label] = 1;

            //write the expanded vertex to the dst:
            NNlabels_row[num_expanded_vertices] = vert_for_expansion.label;
            NNdistances_row[num_expanded_vertices] = vert_for_expansion.dist;
            num_expanded_vertices++;

            //update the heap:
            neighbors = &inst->g[vert_for_expansion.label].front();
            for(int j=0;j<(int)inst->g[vert_for_expansion.label].size();j++)
            {
                if(!expanded_flag[neighbors[j].label])
                    q.updateNode(node(neighbors[j].label,vert_for_expansion.dist+neighbors[j].dist));
            }
        }
    }
}

void weightedLeastSquaresAffineFit(short* labels, float* weights, int count, SparseMatch* matches, Mat& dst)
{
    double sa[6][6]={{0.}}, sb[6]={0.};
    Mat A (6, 6, CV_64F, &sa[0][0]), 
        B (6, 1, CV_64F, sb),
        MM(1, 6, CV_64F);
    Point2f a,b;
    float w;

    for( int i = 0; i < count; i++ )
    {
        a = matches[labels[i]].reference_image_pos;
        b = matches[labels[i]].target_image_pos;
        w = weights[i];

        sa[0][0] += w*a.x*a.x;
        sa[0][1] += w*a.y*a.x;
        sa[0][2] += w*a.x;
        sa[1][1] += w*a.y*a.y;
        sa[1][2] += w*a.y;
        sa[2][2] += w;

        sb[0] += w*a.x*b.x;
        sb[1] += w*a.y*b.x;
        sb[2] += w*b.x;
        sb[3] += w*a.x*b.y;
        sb[4] += w*a.y*b.y;
        sb[5] += w*b.y;
    }

    sa[3][4] = sa[4][3] = sa[1][0] = sa[0][1];
    sa[3][5] = sa[5][3] = sa[2][0] = sa[0][2];
    sa[4][5] = sa[5][4] = sa[2][1] = sa[1][2];

    sa[3][3] = sa[0][0];
    sa[4][4] = sa[1][1];
    sa[5][5] = sa[2][2];

    solve(A, B, MM, DECOMP_EIG);
    MM.reshape(2,3).convertTo(dst,CV_32F);
}

void generateHypothesis(short* labels, int count, RNG& rng, bitset<SHRT_MAX>& is_used, SparseMatch* matches, Mat& dst)
{
    int idx;
    Point2f src_points[3];
    Point2f dst_points[3];
    is_used.reset();

    // randomly get 3 distinct matches
    idx = rng.uniform(0,count-2);
    is_used[idx] = true;
    src_points[0] = matches[labels[idx]].reference_image_pos;
    dst_points[0] = matches[labels[idx]].target_image_pos;

    idx = rng.uniform(0,count-1);
    if (is_used[idx])
        idx = count-2;
    is_used[idx] = true;
    src_points[1] = matches[labels[idx]].reference_image_pos;
    dst_points[1] = matches[labels[idx]].target_image_pos;

    idx = rng.uniform(0,count);
    if (is_used[idx])
        idx = count-1;
    is_used[idx] = true;
    src_points[2] = matches[labels[idx]].reference_image_pos;
    dst_points[2] = matches[labels[idx]].target_image_pos;

    // compute an affine transform:
    getAffineTransform(src_points,dst_points).convertTo(dst,CV_32F);
}

void verifyHypothesis(short* labels, float* weights, int count, SparseMatch* matches, float eps, float lambda, Mat& hypothesis_transform, Mat& old_transform, float& old_weighted_num_inliers)
{
    float* tr = hypothesis_transform.ptr<float>(0);
    Point2f a,b;
    float weighted_num_inliers = -lambda*(tr[0]*tr[0]+tr[1]*tr[1]+tr[3]*tr[3]+tr[4]*tr[4]);;
    for(int i=0;i<count;i++)
    {
        a = matches[labels[i]].reference_image_pos;
        b = matches[labels[i]].target_image_pos;
        if(abs(tr[0]*a.x + tr[1]*a.y + tr[2] - b.x) +
           abs(tr[3]*a.x + tr[4]*a.y + tr[5] - b.y) < eps)
            weighted_num_inliers += weights[i];
    }

    if(weighted_num_inliers>=old_weighted_num_inliers)
    {
        old_weighted_num_inliers = weighted_num_inliers;
        hypothesis_transform.copyTo(old_transform);
    }
}

EdgeAwareInterpolatorImpl::RansacInterpolation_ParBody::RansacInterpolation_ParBody(EdgeAwareInterpolatorImpl& _inst, Mat* _transforms, float* _weighted_inlier_nums, SparseMatch* _matches, int _num_stripes, int _inc):
inst(&_inst), transforms(_transforms), weighted_inlier_nums(_weighted_inlier_nums), num_stripes(_num_stripes), matches(_matches), inc(_inc)
{
    stripe_sz = (int)ceil(inst->match_num/(double)num_stripes);
}

void EdgeAwareInterpolatorImpl::RansacInterpolation_ParBody::operator() (const Range& range) const
{
    if(range.end>range.start+1)
    {
        for(int n=range.start;n<range.end;n++)
            (*this)(Range(n,n+1));
        return;
    }

    int start = std::min(range.start * stripe_sz, inst->match_num);
    int end   = std::min(range.end   * stripe_sz, inst->match_num);
    if(inc<0)
    {
        int tmp = end;
        end = start-1;
        start = tmp-1;
    }

    short* KNNlabels;
    float* KNNdistances;
    bitset<SHRT_MAX> is_used;
    Mat hypothesis_transform;

    short* inlier_labels    = new short[inst->k];
    float* inlier_distances = new float[inst->k];
    float* tr;
    int num_inliers;
    Point2f a,b;

    for(int i=start;i!=end;i+=inc)
    {
        if(inst->g[i].empty())
            continue;

        KNNlabels    = inst->NNlabels.ptr<short>(i);
        KNNdistances = inst->NNdistances.ptr<float>(i);
        if(inc>0) //forward pass
            hal::exp(KNNdistances,KNNdistances,inst->k);

        for(int it=0;it<inst->ransac_interpolation_num_iter;it++)
        {
            generateHypothesis(KNNlabels,inst->k,inst->rngs[range.start],is_used,matches,hypothesis_transform);
            verifyHypothesis(KNNlabels,KNNdistances,inst->k,matches,inst->inlier_eps,inst->regularization_coef,hypothesis_transform,transforms[i],weighted_inlier_nums[i]);
        }

        //propagate hypotheses from neighbors:
        node* neighbors = &inst->g[i].front();
        for(int j=0;j<(int)inst->g[i].size();j++)
        {
            if((inc*neighbors[j].label)<(inc*i) && (inc*neighbors[j].label)>=(inc*start)) //already processed this neighbor
                verifyHypothesis(KNNlabels,KNNdistances,inst->k,matches,inst->inlier_eps,inst->regularization_coef,transforms[neighbors[j].label],transforms[i],weighted_inlier_nums[i]);
        }

        if(inc<0) //backward pass
        {
            // determine inliers and compute a least squares fit:
            tr = transforms[i].ptr<float>(0);
            num_inliers = 0;

            for(int j=0;j<inst->k;j++)
            {
                a = matches[KNNlabels[j]].reference_image_pos;
                b = matches[KNNlabels[j]].target_image_pos;
                if(abs(tr[0]*a.x + tr[1]*a.y + tr[2] - b.x) +
                   abs(tr[3]*a.x + tr[4]*a.y + tr[5] - b.y) < inst->inlier_eps)
                {
                    inlier_labels[num_inliers]    = KNNlabels[j];
                    inlier_distances[num_inliers] = KNNdistances[j];
                    num_inliers++;
                }
            }

            weightedLeastSquaresAffineFit(inlier_labels,inlier_distances,num_inliers,matches,transforms[i]);
        }
    }

    delete[] inlier_labels;
    delete[] inlier_distances;
}

void EdgeAwareInterpolatorImpl::ransacInterpolation(vector<SparseMatch>& matches, Mat& dst_dense_flow)
{
    NNdistances *= (-sigma*sigma);

    Mat* transforms = new Mat[match_num];
    float* weighted_inlier_nums = new float[match_num];
    for(int i=0;i<match_num;i++)
        weighted_inlier_nums[i] = -1E+10F;
    //memset(weighted_inlier_nums,0,match_num*sizeof(float));
    int inc = 1;

    for(int i=0;i<ransac_num_stripes;i++)
        rngs[i] = RNG(0);

    //forward pass:
    parallel_for_(Range(0,ransac_num_stripes),RansacInterpolation_ParBody(*this,transforms,weighted_inlier_nums,&matches.front(),ransac_num_stripes,1));
    //backward pass:
    parallel_for_(Range(0,ransac_num_stripes),RansacInterpolation_ParBody(*this,transforms,weighted_inlier_nums,&matches.front(),ransac_num_stripes,-1));
    

    //construct the final piecewise-affine interpolation:
    short* label_row;
    float* tr;
    for(int i=0;i<h;i++)
    {
        label_row = labels.ptr<short>(i);
        Point2f* dst_row = dst_dense_flow.ptr<Point2f>(i); 
        for(int j=0;j<w;j++)
        {
            tr = transforms[label_row[j]].ptr<float>(0);
            dst_row[j] = Point2f(tr[0]*j+tr[1]*i+tr[2],tr[3]*j+tr[4]*i+tr[5]) - Point2f((float)j,(float)i);
        }
    }

    delete[] transforms;
    delete[] weighted_inlier_nums;
}

CV_EXPORTS_W
Ptr<EdgeAwareInterpolator> createEdgeAwareInterpolator()
{
    return Ptr<EdgeAwareInterpolator>(EdgeAwareInterpolatorImpl::create());
}

}
}