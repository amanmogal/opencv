import numpy as np
import cv2 as cv
import argparse
import os


def calc_coords(bb):
    '''
    The method used to evaluate coordinates of corners of the bounding box.
    Input: tuple (x, y, w, h), where:
    x - "x" coordinates of the top left corner
    y - "y" coordinates of the top left corner
    w - width of the bounding box
    h - the height of the bounding box
    Output: dictionary coordinates contain names of the values as keys and
    numerical values as values
    '''
    xmin = bb[0]
    xmax = bb[0] + bb[2] - 1.0
    ymin = bb[1]
    ymax = bb[1] + bb[3] - 1.0
    cx = bb[0] + (bb[2] + 1.0) / 2
    cy = bb[1] + (bb[3] + 1.0) / 2
    coords = {'xmin': xmin, 'xmax': xmax, 'ymin': ymin, 'ymax': ymax,
              'cx': cx, 'cy': cy}
    return coords


def get_iou(new, gt):
    '''
    During the calculation of intersection over union, we are checking
    numerical value of area_of_overlap, because if it is equal to 0,
    we have no intersection.
    Lists, generated by 'calc_coords', are used as input for these methods.

    Respectively:
    new[0],gt[0] -> xmin, new[1],gt[1] -> xmax, new[2],gt[2] -> ymin
    new[3],gt[3] -> ymax, new[4],gt[4] -> cx, new[5],gt[5] -> cy
    '''
    new_xmin, new_xmax, new_ymin, new_ymax, new_cx, new_cy = new
    gt_xmin, gt_xmax, gt_ymin, gt_ymax, gt_cx, gt_cy = gt
    dx = max(0, min(new_xmax, gt_xmax) - max(new_xmin, gt_xmin))
    dy = max(0, min(new_ymax, gt_ymax) - max(new_ymin, gt_ymin))
    area_of_overlap = dx * dy
    if area_of_overlap != 0:
        area_of_union = (new_xmax - new_xmin) * (new_ymax - new_ymin) + (
            gt_xmax - gt_xmin) * (gt_ymax - gt_ymin) - area_of_overlap
        iou = area_of_overlap / area_of_union
    else:
        iou = 0
    return iou


def get_pr(new, gt):
    '''
    In calculations of precision and normalized precision are used thresholds
    from original TrackingNet paper: for metric calculation TrackingNet using
    lists of thresholds, but here we use only one numerical value of the
    threshold for each metric
    Lists, generated by 'calc_coords', are used as input for these methods.

    Respectively:
    new[0],gt[0] -> xmin, new[1],gt[1] -> xmax, new[2],gt[2] -> ymin
    new[3],gt[3] -> ymax, new[4],gt[4] -> cx, new[5],gt[5] -> cy
    '''
    new_xmin, new_xmax, new_ymin, new_ymax, new_cx, new_cy = new
    gt_xmin, gt_xmax, gt_ymin, gt_ymax, gt_cx, gt_cy = gt
    precision = np.sqrt((new_cx - gt_cx) ** 2 + (new_cy - gt_cy) ** 2)
    return precision


def get_norm_pr(new, gt, gt_bb_w, gt_bb_h):
    '''
    In calculations of precision and normalized precision are used thresholds
    from original TrackingNet paper: for metric calculation TrackingNet using
    lists of thresholds, but here we use only one numerical value of the
    threshold for each metric
    Lists, generated by 'calc_coords', are used as input for these methods.

    Respectively:
    new[0],gt[0] -> xmin, new[1],gt[1] -> xmax, new[2],gt[2] -> ymin
    new[3],gt[3] -> ymax, new[4],gt[4] -> cx, new[5],gt[5] -> cy
    '''
    new_xmin, new_xmax, new_ymin, new_ymax, new_cx, new_cy = new
    gt_xmin, gt_xmax, gt_ymin, gt_ymax, gt_cx, gt_cy = gt
    normalized_precision = np.sqrt(((new_cx - gt_cx) / gt_bb_w) ** 2 + (
        (new_cy - gt_cy) / gt_bb_h) ** 2)
    return normalized_precision


def init_tracker(tracker_name):
    '''
    Method used for initializing of trackers by creating it
    via cv.TrackerX_create().
    Input: string with tracker name.
    Output: dictionary 'config'
    Dictionary 'config' contains trackers names
    as keys and tuple with call method and number of frames for
    reinitialization as values
    '''
    config = {"Boosting": (cv.TrackerBoosting_create(), 500),
              "MIL": (cv.TrackerMIL_create(), 1000),
              "KCF": (cv.TrackerKCF_create(), 1000),
              "MedianFlow": (cv.TrackerMedianFlow_create(), 1000),
              "GOTURN": (cv.TrackerGOTURN_create(), 250),
              "MOSSE": (cv.TrackerMOSSE_create(), 1000),
              "CSRT": (cv.TrackerCSRT_create(), 1000)}
    return config[tracker_name]


def main():
    parser = argparse.ArgumentParser(
        description="Run LaSOT-based benchmark for visual object trackers")
    # As a default argument used name of
    # original dataset folder
    parser.add_argument("--path_to_dataset", type=str,
                        default="LaSOTTesting", help="Full path to LaSOT")
    parser.add_argument("--visualization", action='store_true',
                        help="Showing process of tracking")
    args = parser.parse_args()

    # Creating list with names of videos via reading names from txt file
    video_names = os.path.join(args.path_to_dataset, "testing_set.txt")
    with open(video_names, 'rt') as f:
        list_of_videos = f.read().rstrip('\n').split('\n')
    trackers = [
        'Boosting', 'MIL', 'KCF', 'MedianFlow', 'GOTURN', 'MOSSE', 'CSRT']

    list_iou = []
    list_pr = []
    list_n_pr = []

    # Loop for every tracker
    for tracker_name in trackers:

        print("Tracker name: ", tracker_name)

        iou_video = np.zeros(21)
        pr_video = np.zeros(21)
        n_pr_video = np.zeros(21)
        iou_thr = np.linspace(0, 1, 21)
        pr_thr = np.linspace(0, 50, 21)
        n_pr_thr = np.linspace(0, 0.5, 21)

        # Loop for every video
        for video_name in list_of_videos:

            tracker, frames_for_reinit = init_tracker(tracker_name)
            init_once = False

            print("\tVideo name: " + str(video_name))

            # Open specific video and read ground truth for it
            gt_file = open(os.path.join(args.path_to_dataset, video_name,
                           "groundtruth.txt"), "r")
            gt_bb = gt_file.readline().replace("\n", "").split(",")
            init_bb = gt_bb
            init_bb = tuple([float(b) for b in init_bb])

            # Creating blob from image sequence
            video_sequence = sorted(os.listdir(os.path.join(
                args.path_to_dataset, video_name, "img")))

            # Variables for saving sum of every metric for every frame and
            # every video respectively
            iou_list = []
            pr_list = []
            n_pr_list = []
            frame_counter = len(video_sequence)

            # For every frame in video
            for number_of_the_frame, image in enumerate(video_sequence):
                frame = cv.imread(os.path.join(
                    args.path_to_dataset, video_name, "img", image))
                for i in range(len(gt_bb)):
                    gt_bb[i] = float(gt_bb[i])
                gt_bb = tuple(gt_bb)

                # Condition of tracker`s re-initialization
                if ((number_of_the_frame + 1) % frames_for_reinit == 0) and (
                        number_of_the_frame != 0):

                    tracker, frames_for_reinit = init_tracker(tracker_name)
                    init_once = False
                    init_bb = gt_bb

                if not init_once:
                    init_state = tracker.init(frame, init_bb)
                    init_once = True
                init_state, new_bb = tracker.update(frame)

                # Check for presence of object on the frame
                # If no object on frame, we reduce number of
                # accounted for evaluation frames
                if gt_bb != (0, 0, 0, 0):
                    # Calculation of coordinates of corners and centers
                    # from [x, y, w, h] bounding boxes

                    new_coords = list(calc_coords(new_bb).values())
                    gt_coords = list(calc_coords(gt_bb).values())

                    if args.visualization:
                        cv.rectangle(frame, (int(new_coords[0]), int(
                                     new_coords[2])), (int(new_coords[1]),
                                     int(new_coords[3])), (200, 0, 0))
                        cv.imshow("Tracking", frame)
                        cv.waitKey(1)

                    iou_list.append(get_iou(new_coords, gt_coords))
                    pr_list.append(get_pr(new_coords, gt_coords))
                    n_pr_list.append(get_norm_pr(
                        new_coords, gt_coords, gt_bb[2], gt_bb[3]))
                else:
                    frame_counter -= 1

                # Setting as ground truth bounding box from next frame
                gt_bb = gt_file.readline().replace("\n", "").split(",")

            # Calculating mean arithmetic value for specific video
            iou_video += (np.fromiter([sum(i >= thr for i in iou_list).astype(
                float) / frame_counter for thr in iou_thr], dtype=float))
            pr_video += (np.fromiter([sum(i <= thr for i in pr_list).astype(
                float) / frame_counter for thr in pr_thr], dtype=float))
            n_pr_video += (np.fromiter([sum(i <= thr for i in n_pr_list).astype(
                float) / frame_counter for thr in n_pr_thr], dtype=float))

        iou_avg = np.array(iou_video) / len(list_of_videos)
        pr_avg = np.array(pr_video) / len(list_of_videos)
        n_pr_avg = np.array(n_pr_video) / len(list_of_videos)

        iou = np.trapz(iou_avg, x=iou_thr) / iou_thr[-1]
        pr = np.trapz(pr_avg, x=pr_thr) / pr_thr[-1]
        n_pr = np.trapz(n_pr_avg, x=n_pr_thr) / n_pr_thr[-1]

        list_iou.append('%.4f' % iou)
        list_pr.append('%.4f' % pr)
        list_n_pr.append('%.4f' % n_pr)

    titles = ["Names:", "IoU:", "Precision:", "N.Precision:"]
    data = [titles] + list(zip(trackers, list_iou, list_pr, list_n_pr))
    for number, for_tracker in enumerate(data):
        line = '|'.join(str(x).ljust(20) for x in for_tracker)
        print(line)
        if number == 0:
            print('-' * len(line))


if __name__ == '__main__':
    main()
