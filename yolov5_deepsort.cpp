#include <mutex>
#include <stdio.h>
#include <string.h>
#include <string>
#include <thread>
#include <unistd.h>

#include "common.h"
#include "deepsort.h"
#include "detect.h"
#include "mytime.h"
#include "videoio.h"

using namespace std;

bool add_head = false;
// string PROJECT_DIR = "/home/wjp/codes/yolov5_Deepsort_rknn";

// string YOLO_MODEL_PATH = PROJECT_DIR + "/model/best.rknn";
// string SORT_MODEL_PATH = PROJECT_DIR + "/model/osnet_x0_25_market.rknn";

// string VIDEO_PATH = PROJECT_DIR + "/data/M0201.mp4";
// string VIDEO_SAVEPATH = PROJECT_DIR + "/data/M0201_results.mp4";

/*
string YOLO_MODEL_PATH = PROJECT_DIR + "/model/best_nofocus_relu.rknn";
string SORT_MODEL_PATH = PROJECT_DIR + "/model/osnet_x0_25_market.rknn";

string VIDEO_PATH = PROJECT_DIR + "/data/DJI_0001_S_cut.mp4";
string VIDEO_SAVEPATH = PROJECT_DIR + "/data/results.mp4";
*/

// 各任务进行状态序号
video_property video_probs; // 视频属性类
int idxInputImage = 0;      // image index of input video
int idxOutputImage = 0;     // image index of output video
int idxTrackImage = 0;      // 目标追踪下一帧要处理的对象
bool bReading = true;       // flag of input
bool bDetecting = true;     // Detect是否完成
bool bTracking = true;      // Track是否完成
double start_time;          // Video Detection开始时间
double end_time;            // Video Detection结束时间

// 多线程控制相关
vector<cv::Mat> imagePool;     // video cache
mutex mtxQueueInput;           // mutex of input queue
queue<input_image> queueInput; // input queue
mutex mtxQueueDetOut;
queue<imageout_idx> queueDetOut; // output queue
mutex mtxQueueOutput;
queue<imageout_idx> queueOutput; // output queue 目标追踪输出队列

void videoRead(const char *video_name, int cpuid);
void videoResize(int cpuid);
void videoWrite(const char *save_path, int cpuid);

int main(int argc, char **argv)
{

    if (argc != 4)
    {
        printf("Usage: %s <rknn model> <jpg> \n", argv[0]);
        return -1;
    }
    char *yolo_model_name = NULL;
    yolo_model_name = (char *)argv[1];
    char *sort_model_name = NULL;
    sort_model_name = (char *)argv[2];
    char *video_name = NULL;
    video_name = (char *)argv[3];
    std::string result = "./result.mp4";

    class Yolo detect1(yolo_model_name, 4, RKNN_NPU_CORE_0, 1, 3);
    class Yolo detect2(yolo_model_name, 5, RKNN_NPU_CORE_1, 1, 3);
    class DeepSort track(sort_model_name, 1, 512, 6, RKNN_NPU_CORE_2);

    const int thread_num = 5;
    std::array<thread, thread_num> threads;
    videoRead(result.c_str(), 7);
    // used CPU: 0, 4, 5, 6, 7
    threads = {
        thread(&Yolo::detect_process, &detect1), // 类成员函数特殊写法
        thread(&Yolo::detect_process, &detect2),
        thread(&DeepSort::track_process, &track),
        thread(videoResize, 7),
        thread(videoWrite, result.c_str(), 0),
    };
    for (int i = 0; i < thread_num; i++)
        threads[i].join();
    printf("Video detection mean cost time(ms): %f\n",
           (end_time - start_time) / video_probs.Frame_cnt);
    return 0;
}
