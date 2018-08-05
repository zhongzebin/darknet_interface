#include "detector.h"

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <time.h>
#include <fstream>
#include <vector>

#ifdef OPENCV
#include "opencv2/core/types_c.h"
#include <opencv2/highgui/highgui.hpp>  
#include <opencv2/imgproc/imgproc.hpp>  
#include <opencv2/core/core.hpp> 
#include <thread>

int FRAME_WINDOW_WIDTH = 320;
int FRAME_WINDOW_HEIGHT = 240;
using namespace std;
using namespace cv;

void get_Beijing_time(int* year, int* month, int* day, int* hour, int* min, double* sec)
{
    double raw_time = what_is_the_time_now();
    double h, m, s;
    h = 24 * (raw_time/(60*60*24) - ((int)raw_time/(60*60*24)));
    m = 60 * (h - (int)(h));
    s  = 60 * (m - (int)(m));
    *hour = (int)h;
    *min = (int)m;
    *sec  = s;
    time_t t; 
    struct tm * a; 
    time(&t); 
    a=localtime(&t); 
    *year = 1900+a->tm_year;
    *month = 1+a->tm_mon;
    *day = a->tm_mday;
}

void save_img_by_time(Mat image)
{
    int year, month, day, hour, min;
    double sec;
    get_Beijing_time(&year, &month, &day, &hour, &min, &sec);
    String s_month = (month<10?"0":"")+to_string(month);
    String s_day = (day<10?"0":"")+to_string(day);
    String s_hour = (hour<10?"0":"")+to_string(hour);
    String s_min= (min<10?"0":"")+to_string(min);
    String s_sec = (sec<10?"0":"")+to_string(sec);// more accurate
    // String s_sec = (sec<10?"0":"")+to_string((int)sec);
    // String save_path = "cap/"+to_string(year)+s_month+s_day+"_"+s_hour+s_min+s_sec+".jpg";// exact to the second
    String save_path = "cap/"+to_string(year)+s_month+s_day+"_"+s_hour+s_min+".jpg";// exact to the minute
    ifstream in(save_path);
    if(!in)
        imwrite(save_path, image);
}

void print_detections(float* detections, int num_output_class)
{
    for(int i = 0; i < num_output_class; i++)
    {
        cout.width(2), cout << (int)detections[i*6+0] << ": ";
        cout.width(5), cout << to_string((int)(round(detections[i*6+1]*100)))+"%";
        cout.width(12),cout << "(left_x: "+to_string((int)round(detections[i*6+2]));
        cout.width(12),cout << "  top_y: "+to_string((int)round(detections[i*6+3]));
        cout.width(12),cout << "  width: "+to_string((int)round(detections[i*6+4]));
        cout.width(13),cout << "  height: "+to_string((int)round(detections[i*6+5])) << ")" << endl;
    }
}

bool is_target_detected(float* detections, int num_output_class, vector<int> target_class_index_list)
{
    for(int i = 0; i < num_output_class; i++)
    {
        for(int j = 0; j < target_class_index_list.size(); j++)
            if(detections[i*6+0] == target_class_index_list[j])
                return true;
    }
    return false;
}

void detect_mat(Mat frame_detect, float* detections_output, int* num_output_class, double* time_consumed, float thresh, float hier_thresh, vector<int> target_class_index_list)
{
    double time = what_is_the_time_now();
    IplImage input = IplImage(frame_detect);// convert image format from mat to iplimage
    
    // do detect in an iplimage converted from mat
    float* detections = test_detector_cv(&input, thresh, hier_thresh, num_output_class);
    for(int i = 0; i < *num_output_class; i++)
    {
        detections_output[i*6+0] = detections[i*6+0];// ith detection's category
        detections_output[i*6+1] = detections[i*6+1];// ith detection's confidence score
        detections_output[i*6+2] = detections[i*6+2];// ith detection's top-left x-coordinate of bbox
        detections_output[i*6+3] = detections[i*6+3];// ith detection's top-left y-coordinate of bbox
        detections_output[i*6+4] = detections[i*6+4];// ith detection's width of bbox
        detections_output[i*6+5] = detections[i*6+5];// ith detection's height of bbox
    }
    // --- do something ---
    print_detections(detections, *num_output_class);
    // if(is_target_detected(detections_output, * num_output_class, target_class_index_list))
    {
        // save_img_by_time(frame_detect);
    }

    if(time_consumed)
        *time_consumed = (what_is_the_time_now() - time);
}

void show_detections(Mat frame, float* detections, int num_output_class, Rect detections_rect, vector<int> target_class_index_list, bool is_show_image, bool is_show_detections)
{
    if(is_show_image)
    {
        if(is_show_detections)
        {
            String info;
            for(int i = 0; i < num_output_class; i++)
            {
                for(int j = 0; j < target_class_index_list.size(); j++)
                {
                    if(detections[i*6+0] == target_class_index_list[j])
                    {
                        detections_rect.x = detections[i*6+2];
                        detections_rect.y = detections[i*6+3];
                        detections_rect.width = detections[i*6+4];
                        detections_rect.height = detections[i*6+5];
                        if(detections[i*6+0] == 0) // person
                            rectangle(frame, detections_rect, CV_RGB(255, 0, 0), 4, 8, 0);
                        else if(detections[i*6+0] == 62 || detections[i*6+0] == 63) // tvmonitor && laptop
                            rectangle(frame, detections_rect, CV_RGB(0, 0, 255), 4, 8, 0);
                        else if(detections[i*6+0] == 28 || detections[i*6+0] == 56)//  suitcase && chair
                            rectangle(frame, detections_rect, CV_RGB(0, 255, 0), 4, 8, 0);
                        else
                            rectangle(frame, detections_rect, CV_RGB(0, 0, 0), 4, 8, 0);
                        info = "cls:"+ to_string(int(detections[i*6+0])) +", conf:"+ to_string((int)round(detections[i*6+1]*100)) +"%";
                        putText(frame, info, Point(detections_rect.x, detections_rect.y-10), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0), 2, 8);
                    }
                }
            }
        }
        namedWindow("detections", cv::WINDOW_AUTOSIZE);
        imshow("detections", frame);
        waitKey(1);
    }
}
#endif

int main(int argc, char** argv)
{
    vector<int> target_class_index_list;
    int num_target;
    cout.setf(std::ios::left);
    if (argc < 6)
    {
        cout << "Usage: " << endl << "  $ "
        << argv[0] << " ${cfg_path} ${weight_path} ${thresh} ${webcam_index} ${target_class_index_list}" << endl << endl
        << "${target_class_index_list} is an integer vector in which stored class index of targets you want to detected." << endl
        << "For instance, you can detect person(0), cat(15) and dog(16) using model trained on MSCOCO(can be downloaded in https://pjreddie.com/darknet/yolo/) through camera 0 by running:" << endl << "  $ "
        << argv[0] << " ../../darknet/cfg/yolov3.cfg ../../darknet/weights/yolov3.weights 0.5 0 0 15 16" << endl
        << endl;
        return -1;
    }
    else
        num_target = argc - 5;
    // ====== init ======
    char *cfgfile = argv[1];
    char *weightfile = argv[2];
    float thresh = atof(argv[3]);
    VideoCapture cap = VideoCapture(atoi(argv[4]));// init camera
    for(int i = 0; i < num_target; i++)
        target_class_index_list.push_back(atoi(argv[5+i]));
    float hier_thresh = 0.5;
    int num_output_class = 0;
    double time_consumed = 0;
    
    float *detections;
    Rect detections_rect;


    bool is_detect_in_thread = false;
    bool is_show_image = true; // shut down for more stable detection when is_detect_in_thread==true
    bool is_show_detections = true;

    detector_init(cfgfile, weightfile);

    // ====== detect in webcam only ======
#ifndef OPENCV
    cout << "need OpenCV!" << endl;
    return -1;
#endif
    if(!cap.isOpened())
    {
        cout << "No video stream captured!" << endl;
        return -1;
    }
    Mat frame;
    detections = (float*)calloc(255*6, sizeof(float));
    while(cap.read(frame))
    {
        // --- detect objects ---
        if(!is_detect_in_thread)
        {
            cout << endl << "------ detect begin ------" << endl;
            detect_mat(frame, detections, &num_output_class, &time_consumed, thresh, hier_thresh, target_class_index_list);
            cout << "time_consumed: " << (float)time_consumed 
                << "s, frame_rate: " << 1/(float)time_consumed << " frame/s" << endl;
        }
        else// do detect in a background thread
        {
            if(time_consumed >= 0)
            {
                cout << "time_consumed: " << (float)time_consumed 
                     << "s, frame_rate: " << 1/(float)time_consumed << " frame/s" << endl;
                cout << endl << "------ detect begin ------" << endl;
                thread t(detect_mat, frame, detections, &num_output_class, &time_consumed, thresh, hier_thresh, target_class_index_list);
                t.detach();
                time_consumed = -1;
            }
        }
        
        // ------ do something if target detected ------
        if(is_target_detected(detections, num_output_class, target_class_index_list))
        {
            save_img_by_time(frame);
        }
        
        // --- show detections ---
        show_detections(frame, detections, num_output_class, detections_rect, target_class_index_list, is_show_image, is_show_detections);
        if(waitKey(30) >=0)
            break;
    }

    // ====== uninit ======
    detector_uninit();

    return 0;
}
