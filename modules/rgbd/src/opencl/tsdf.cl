// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html

// This code is also subject to the license terms in the LICENSE_KinectFusion.md file found in this module's directory

__kernel void integrate(__global const char * depthptr,
                        int depth_step, int depth_offset,
                        int depth_rows, int depth_cols,
                        __global float2 * volumeptr,
                        const float16 vol2camMatrix,
                        const float4 voxelSize4,
                        const int4 volResolution4,
                        const int4 volDims4,
                        const float fx, const float fy,
                        const float cx, const float cy,
                        const float dfac,
                        const float truncDist,
                        const int maxWeight)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    const int3 volResolution = volResolution4.xyz;

    if(x >= volResolution.x || y >= volResolution.y)
        return;

    // coord-independent constants
    const int3 volDims = volDims4.xyz;
    const float2 limits = (float2)(depth_cols-1, depth_rows-1);

    const float4 vol2cam0 = vol2camMatrix.s0123;
    const float4 vol2cam1 = vol2camMatrix.s4567;
    const float4 vol2cam2 = vol2camMatrix.s89ab;

    const float2 fxy = (float2)(fx, fy);
    const float2 cxy = (float2)(cx, cy);

    const float truncDistInv = 1.f/truncDist;

    const float3 voxelSize = voxelSize4.xyz;

    // optimization of camSpace transformation (vector addition instead of matmul at each z)
    float4 inPt = (float4)(x*voxelSize.x, y*voxelSize.y, 0, 1);
    float3 basePt = (float3)(dot(vol2cam0, inPt),
                             dot(vol2cam1, inPt),
                             dot(vol2cam2, inPt));

    float3 camSpacePt = basePt;

    // zStep == vol2cam*(float3(x, y, 1)*voxelSize) - basePt;
    float3 zStep = ((float3)(vol2cam0.z, vol2cam1.z, vol2cam2.z))*voxelSize;

    int volYidx = x*volDims.x + y*volDims.y;

    int startZ, endZ;
    if(fabs(zStep.z) > 1e-5)
    {
        int baseZ = convert_int(-basePt.z / zStep.z);
        if(zStep.z > 0)
        {
            startZ = baseZ;
            endZ = volResolution.z;
        }
        else
        {
            startZ = 0;
            endZ = baseZ;
        }
    }
    else
    {
        if(basePt.z > 0)
        {
            startZ = 0; endZ = volResolution.z;
        }
        else
        {
            // z loop shouldn't be performed
            //startZ = endZ = 0;
            return;
        }
    }

    startZ = max(0, startZ);
    endZ = min(volResolution.z, endZ);

    for(int z = startZ; z < endZ; z++)
    {
        // optimization of the following:
        //float3 camSpacePt = vol2cam * ((float3)(x, y, z)*voxelSize);
        camSpacePt += zStep;

        if(camSpacePt.z <= 0)
            continue;

        float3 camPixVec = camSpacePt / camSpacePt.z;
        float2 projected = mad(camPixVec.xy, fxy, cxy);

        float v;
        // bilinearly interpolate depth at projected
        if(all(projected >= 0) && all(projected < limits))
        {
            float2 ip = floor(projected);
            int xi = ip.x, yi = ip.y;

            __global const float* row0 = (__global const float*)(depthptr + depth_offset +
                                                                 (yi+0)*depth_step);
            __global const float* row1 = (__global const float*)(depthptr + depth_offset +
                                                                 (yi+1)*depth_step);

            float v00 = row0[xi+0];
            float v01 = row0[xi+1];
            float v10 = row1[xi+0];
            float v11 = row1[xi+1];
            float4 vv = (float4)(v00, v01, v10, v11);

            // assume correct depth is positive
            if(all(vv > 0))
            {
                float2 t = projected - ip;
                float2 vf = mix(vv.xz, vv.yw, t.x);
                v = mix(vf.s0, vf.s1, t.y);
            }
            else
                continue;
        }
        else
            continue;

        if(v == 0)
            continue;

        float pixNorm = length(camPixVec);

        // difference between distances of point and of surface to camera
        float sdf = pixNorm*(v*dfac - camSpacePt.z);
        // possible alternative is:
        // float sdf = length(camSpacePt)*(v*dfac/camSpacePt.z - 1.0);

        if(sdf >= -truncDist)
        {
            float tsdf = fmin(1.0f, sdf * truncDistInv);
            int volIdx = volYidx + z*volDims.z;

            float2 voxel = volumeptr[volIdx];
            float value  = voxel.s0;
            int weight = as_int(voxel.s1);

            // update TSDF
            value = (value*weight + tsdf) / (weight + 1);
            weight = min(weight + 1, maxWeight);

            voxel.s0 = value;
            voxel.s1 = as_float(weight);
            volumeptr[volIdx] = voxel;
        }
    }
}


inline float interpolateVoxel(float3 p, __global const float2* volumePtr,
                              int3 volDims, int8 neighbourCoords)
{
    float3 fip = floor(p);
    int3 ip = convert_int(fip);
    float3 t = p - fip;

    int3 cmul = volDims*ip;
    int coordBase = cmul.x + cmul.y + cmul.z;
    int nco[8];
    vstore8(neighbourCoords + coordBase, 0, nco);

    float vaz[8];
    for(int i = 0; i < 8; i++)
        vaz[i] = volumePtr[nco[i]].s0;

    float8 vz = vload8(0, vaz);

    float4 vy = mix(vz.s0246, vz.s1357, t.z);
    float2 vx = mix(vy.s02, vy.s13, t.y);
    return mix(vx.s0, vx.s1, t.x);
}

inline float3 getNormalVoxel(float3 p, __global const float2* volumePtr,
                             int3 volResolution, int3 volDims, int8 neighbourCoords)
{
    if(any(p < 1) || any(p >= convert_float(volResolution - 2)))
        return nan((uint)0);

    float3 fip = floor(p);
    int3 ip = convert_int(fip);
    float3 t = p - fip;

    int3 cmul = volDims*ip;
    int coordBase = cmul.x + cmul.y + cmul.z;
    int nco[8];
    vstore8(neighbourCoords + coordBase, 0, nco);

    int arDims[3];
    vstore3(volDims, 0, arDims);
    float an[3];
    for(int c = 0; c < 3; c++)
    {
        int dim = arDims[c];

        float vaz[8];
        for(int i = 0; i < 8; i++)
            vaz[i] = volumePtr[nco[i] + dim].s0 -
                     volumePtr[nco[i] - dim].s0;

        float8 vz = vload8(0, vaz);

        float4 vy = mix(vz.s0246, vz.s1357, t.z);
        float2 vx = mix(vy.s02, vy.s13, t.y);

        an[c] = mix(vx.s0, vx.s1, t.x);
    }

    //gradientDeltaFactor is fixed at 1.0 of voxel size
    return fast_normalize(vload3(0, an));
}

typedef float4 ptype;

__kernel void raycast(__global char * pointsptr,
                      int points_step, int points_offset,
                      int points_rows, int points_cols,
                      __global char * normalsptr,
                      int normals_step, int normals_offset,
                      int normals_rows, int normals_cols,
                      __global const float2 * volumeptr,
                      __global const float * vol2camptr,
                      __global const float * cam2volptr,
                      const float fxinv, const float fyinv,
                      const float cx, const float cy,
                      const float4 boxDown4,
                      const float4 boxUp4,
                      const float tstep,
                      const float4 voxelSize4,
                      const int4 volResolution4,
                      const int4 volDims4,
                      const int8 neighbourCoords
                      )
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if(x >= points_cols || y >= points_rows)
        return;

    // coordinate-independent constants

    __global const float* cm = cam2volptr;
    const float3 camRot0  = vload4(0, cm).xyz;
    const float3 camRot1  = vload4(1, cm).xyz;
    const float3 camRot2  = vload4(2, cm).xyz;
    const float3 camTrans = (float3)(cm[3], cm[7], cm[11]);

    __global const float* vm = vol2camptr;
    const float3 volRot0  = vload4(0, vm).xyz;
    const float3 volRot1  = vload4(1, vm).xyz;
    const float3 volRot2  = vload4(2, vm).xyz;
    const float3 volTrans = (float3)(vm[3], vm[7], vm[11]);

    const float2 fixy = (float2)(fxinv, fyinv);
    const float2 cxy  = (float2)(cx, cy);

    const float3 boxDown = boxDown4.xyz;
    const float3 boxUp   = boxUp4.xyz;
    const int3   volDims = volDims4.xyz;

    const int3 volResolution = volResolution4.xyz;

    const float3 voxelSize = voxelSize4.xyz;
    const float3 invVoxelSize = native_recip(voxelSize);

    // kernel itself

    float3 point  = nan((uint)0);
    float3 normal = nan((uint)0);

    float3 orig = camTrans;

    // get direction through pixel in volume space:
    // 1. reproject (x, y) on projecting plane where z = 1.f
    float3 planed = (float3)(((float2)(x, y) - cxy)*fixy, 1.f);

    // 2. rotate to volume space
    planed = (float3)(dot(planed, camRot0),
                      dot(planed, camRot1),
                      dot(planed, camRot2));

    // 3. normalize
    float3 dir = fast_normalize(planed);

    // compute intersection of ray with all six bbox planes
    float3 rayinv = native_recip(dir);
    float3 tbottom = rayinv*(boxDown - orig);
    float3 ttop    = rayinv*(boxUp   - orig);

    // re-order intersections to find smallest and largest on each axis
    float3 minAx = min(ttop, tbottom);
    float3 maxAx = max(ttop, tbottom);

    // near clipping plane
    const float clip = 0.f;
    float tmin = max(max(max(minAx.x, minAx.y), max(minAx.x, minAx.z)), clip);
    float tmax =     min(min(maxAx.x, maxAx.y), min(maxAx.x, maxAx.z));

    // precautions against getting coordinates out of bounds
    tmin = tmin + tstep;
    tmax = tmax - tstep;

    if(tmin < tmax)
    {
        // interpolation optimized a little
        orig *= invVoxelSize;
        dir  *= invVoxelSize;

        float3 rayStep = dir*tstep;
        float3 next = (orig + dir*tmin);
        float f = interpolateVoxel(next, volumeptr, volDims, neighbourCoords);
        float fnext = f;

        // raymarch
        int steps = 0;
        int nSteps = floor(native_divide(tmax - tmin, tstep));
        bool stop = false;
        for(int i = 0; i < nSteps; i++)
        {
            // fix for wrong steps counting
            if(!stop)
            {
                next += rayStep;

                // fetch voxel
                int3 ip = convert_int(round(next));
                int3 cmul = ip*volDims;
                int coord = cmul.x + cmul.y + cmul.z;
                fnext = volumeptr[coord].s0;

                if(fnext != f)
                {
                    fnext = interpolateVoxel(next, volumeptr, volDims, neighbourCoords);

                    // when ray crosses a surface
                    if(signbit(f) != signbit(fnext))
                    {
                        stop = true; continue;
                    }

                    f = fnext;
                }
                steps++;
            }
        }

        // if ray penetrates a surface from outside
        // linearly interpolate t between two f values
        if(f > 0 && fnext < 0)
        {
            float3 tp = next - rayStep;
            float ft   = interpolateVoxel(tp,   volumeptr, volDims, neighbourCoords);
            float ftdt = interpolateVoxel(next, volumeptr, volDims, neighbourCoords);
            // float t = tmin + steps*tstep;
            // float ts = t - tstep*ft/(ftdt - ft);
            float ts = tmin + tstep*(steps - native_divide(ft, ftdt - ft));

            // avoid division by zero
            if(!isnan(ts) && !isinf(ts))
            {
                float3 pv = orig + dir*ts;
                float3 nv = getNormalVoxel(pv, volumeptr, volResolution, volDims, neighbourCoords);

                if(!any(isnan(nv)))
                {
                    //convert pv and nv to camera space
                    normal = (float3)(dot(nv, volRot0),
                                      dot(nv, volRot1),
                                      dot(nv, volRot2));
                    // interpolation optimized a little
                    pv *= voxelSize;
                    point = (float3)(dot(pv, volRot0),
                                     dot(pv, volRot1),
                                     dot(pv, volRot2)) + volTrans;
                }
            }
        }
    }

    __global float* pts = (__global float*)(pointsptr  +  points_offset + y*points_step  + x*sizeof(ptype));
    __global float* nrm = (__global float*)(normalsptr + normals_offset + y*normals_step + x*sizeof(ptype));
    vstore4((float4)(point,  0), 0, pts);
    vstore4((float4)(normal, 0), 0, nrm);
}
