import os
os.add_dll_directory(r'G:\Lib\install\opencv\x64\vc15\bin')
os.add_dll_directory(r'G:\Lib\install\vtk\bin')
os.add_dll_directory(r'G:\Lib\install\ceres-solver\bin')
os.add_dll_directory(r'G:\Lib\install\glog\bin')
os.add_dll_directory(r'F:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.2\bin')
os.add_dll_directory(r'F:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.2')

import cv2 as cv
import numpy as np

v = cv.viz_Viz3d("Coordinate Frame")

pw = cv.viz_ParamWidget()
pw.p1 = (-1.0,-1.0,-1.0)
pw.p2 = (1.0,1.0,1.0)
pw.thickness = 0.2
pw.widget_type = cv.viz.WIDGET_WCoordinateSystem
pw.blue = 255
pw.green = 192
pw.red = 128
pw.widget_name = "Coordinate Widget"
pw.rot_vec = np.zeros(shape=(3,1),dtype=np.float64)
pw.trans_vec=np.zeros(shape=(3,1),dtype=np.float64)
v.showWidget(pw)

p_cube = cv.viz_ParamWidget()
p_cube.p1 = (0.5,0.5,0.0)
p_cube.p2 = (0.0,0.0,-0.5)
p_cube.thickness = 4.0
p_cube.widget_type = cv.viz.WIDGET_WCube
p_cube.blue = 255
p_cube.green = 0
p_cube.red = 0
p_cube.widget_name = "Cube Widget"
p_cube.rot_vec = np.zeros(shape=(1,3),dtype=np.float32)
p_cube.trans_vec= np.zeros(shape=(3,1),dtype=np.float32)
p_cube.trans_vec[0,0]=2
v.showWidget(p_cube)
pi = np.arccos(-1)
translation_phase = 0.0
translation = 0.0
rot_mat = np.zeros(shape=(3, 3), dtype=np.float32)
p_cube.pose = np.zeros(shape=(4, 4), dtype=np.float32)
p_cube.pose[3, 3] = 1
while not v.wasStopped():
    p_cube.rot_vec[0, 0] += pi * 0.01
    p_cube.rot_vec[0, 1] += pi * 0.01
    p_cube.rot_vec[0, 2] += pi * 0.01
    translation_phase += pi * 0.01
    translation = np.sin(translation_phase)
    cv.Rodrigues(p_cube.rot_vec, rot_mat)
    p_cube.pose[0:3,0:3] = rot_mat
    p_cube.pose[0:3,3] = translation
    v.setWidgetPose(p_cube)
    v.spinOnce(1, True)
print("Last event loop is over")
