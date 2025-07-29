#include "videoio.h"
#include "common.h"
#include "resize.h"
#include <thread>
#include <unistd.h>

using namespace std;

extern video_property video_probs;
extern vector<cv::Mat> imagePool;
extern mutex mtxQueueInput;
extern queue<input_image> queueInput; // input queue client
extern mutex mtxQueueDetOut;
extern queue<imageout_idx> queueDetOut; // Det output queue
extern mutex mtxQueueOutput;
extern queue<imageout_idx> queueOutput; // 目标追踪输出队列

extern bool add_head;
extern bool bReading;   // flag of input
extern bool bDetecting; // 目标检测进程状态
extern bool bTracking;
extern int idxInputImage; // image index of input video

/*---------------------------------------------------------
	读视频 缓存在imagePool
	video_name: 视频路径
	cpuid:		绑定到某核
----------------------------------------------------------*/
void videoRead(const char *video_name, int cpuid)
{
    // int initialization_finished = 1;
    cpu_set_t mask;

    CPU_ZERO(&mask);
    CPU_SET(cpuid, &mask);

    if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0)
        cerr << "set thread affinity failed" << endl;

    printf("Bind videoReadClient process to CPU %d\n", cpuid);

    cv::VideoCapture video;
    if (!video.open(video_name))
    {
        cout << "Fail to open " << video_name << endl;
        return;
    }

    video_probs.Frame_cnt = video.get(CV_CAP_PROP_FRAME_COUNT);
    video_probs.Fps = video.get(CV_CAP_PROP_FPS);
    video_probs.Video_width = video.get(CV_CAP_PROP_FRAME_WIDTH);
    video_probs.Video_height = video.get(CV_CAP_PROP_FRAME_HEIGHT);
    video_probs.Video_fourcc = video.get(CV_CAP_PROP_FOURCC);

    bReading = true; //读写状态标记
    while (true)
    {
        cv::Mat img_src;
        bool success = video.read(img_src);

        if (!success)
        {
            // 检查是否为文件结束
            if (video.get(cv::CAP_PROP_POS_FRAMES) >= video.get(cv::CAP_PROP_FRAME_COUNT))
            {
                cout << "Video ended normally" << endl;
                break;
            }

            // 记录错误详情
            cerr << "Error reading frame at position: "
                 << video.get(cv::CAP_PROP_POS_MSEC) << " ms\n";

            // 尝试恢复
            int retry = 0;
            while (retry++ < 5 && !success)
            {
                std::this_thread::sleep_for(chrono::milliseconds(100)); // 等待100ms
                success = video.read(img_src);
            }

            if (!success)
            {
                cerr << "Failed after 5 retries. Releasing video." << endl;
                video.release();
                break;
            }
        }

        imagePool.emplace_back(img_src.clone()); // 使用clone避免引用问题
    }
    cout << "VideoRead is over." << endl;
    cout << "Video Total Length: " << imagePool.size() << "\n";
}

/*---------------------------------------------------------
	调整视频尺寸
	cpuid:		绑定到某核
----------------------------------------------------------*/
void videoResize(int cpuid)
{
    // int initialization_finished = 1;
    rga_buffer_t src;
    rga_buffer_t dst;
    im_rect src_rect;
    im_rect dst_rect;
    memset(&src_rect, 0, sizeof(src_rect));
    memset(&dst_rect, 0, sizeof(dst_rect));
    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));

    cpu_set_t mask;

    CPU_ZERO(&mask);
    CPU_SET(cpuid, &mask);

    if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0)
        cerr << "set thread affinity failed" << endl;

    printf("Bind videoTransClient process to CPU %d\n", cpuid);

    PreResize pre_do(NET_INPUTHEIGHT, NET_INPUTWIDTH, NET_INPUTCHANNEL);
    bReading = true; //读写状态标记
    cout << "total length of video: " << video_probs.Frame_cnt << "\n";
    while (1)
    {
        // 如果读不到图片 或者 bReading 不在读取状态则跳出
        if (!bReading || idxInputImage >= video_probs.Frame_cnt)
        {
            break;
        }
        cv::Mat img_src = imagePool[idxInputImage];
        cv::Mat img = img_src.clone();
        cv::cvtColor(img_src, img, cv::COLOR_BGR2RGB);

        cv::Mat img_pad;
        // resize(img, img_pad, cv::Size(640, 640), 0, 0, 1);

        // if (add_head)
        // {
        //     // adaptive head
        // }
        // else
        // {
        //     // rga resize

        //     void *resize_buf = malloc(NET_INPUTHEIGHT * NET_INPUTWIDTH * NET_INPUTCHANNEL);
        //     src = wrapbuffer_virtualaddr((void *)img.data, img.cols, img.rows, RK_FORMAT_RGB_888);
        //     dst = wrapbuffer_virtualaddr((void *)resize_buf, NET_INPUTWIDTH, NET_INPUTHEIGHT, RK_FORMAT_RGB_888);
        // }
        float img_wh_ratio = (float)img.cols / (float)img.rows;
        float input_wh_ratio = (float)NET_INPUTWIDTH / (float)NET_INPUTHEIGHT;
        float resize_scale;
        int resize_width, resize_height;
        int h_pad = 0, w_pad = 0;

        if (img_wh_ratio >= input_wh_ratio)
        {
            // pad height dim
            resize_scale = (float)NET_INPUTWIDTH / (float)img.cols;
            resize_width = NET_INPUTWIDTH;
            resize_height = (int)((float)img.rows * resize_scale);
            h_pad = (NET_INPUTHEIGHT - resize_height) / 2;
        }
        else
        {
            // pad width dim
            resize_scale = (float)NET_INPUTHEIGHT / (float)img.rows;
            resize_width = (int)((float)img.cols * resize_scale);
            resize_height = NET_INPUTHEIGHT;
            w_pad = (NET_INPUTWIDTH - resize_width) / 2;
        }

        // 调整大小并填充
        cv::resize(img, img_pad, cv::Size(resize_width, resize_height));
        cv::copyMakeBorder(img_pad, img_pad, h_pad, h_pad, w_pad, w_pad, cv::BORDER_CONSTANT, cv::Scalar(128, 128, 128));

        mtxQueueInput.lock();
        queueInput.push(input_image(idxInputImage, img_src, img_pad, resize_scale, w_pad, h_pad));
        mtxQueueInput.unlock();
        idxInputImage++;
    }
    bReading = false;
    cout << "VideoResize is over." << endl;
    cout << "Resize Video Total Length: " << queueInput.size() << "\n";
}

/*
	预处理的缩放比例
	在不丢失原图比例的同时，尽可能的伸缩；同时为了保证检测效果，只允许缩放，不允许放大。
	fx = 1 沿x轴缩放
	fy = 1 沿y轴缩放
*/
void get_max_scale(int input_width, int input_height, int net_width, int net_height, double &fx, double &fy)
{
    double img_wh_ratio = (double)input_width / (double)input_height;
    double input_wh_ratio = (double)net_width / (double)net_height;
    if (img_wh_ratio >= input_wh_ratio)
    {
        // 缩放相同倍数 w 先到达边界
        fx = (double)net_width / input_width;
        fy = (double)net_width / input_width;
    }
    else
    {
        fx = (double)net_height / input_height;
        fy = (double)net_height / input_height;
    }
    return;
}

void videoWrite(const char *save_path, int cpuid)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpuid, &mask);

    if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0)
        cerr << "set thread affinity failed" << endl;

    printf("[VideoWrite] Thread bound to CPU %d\n", cpuid);

    cv::VideoWriter vid_writer;
    int waitCount = 0;

    // 第一个等待循环的诊断日志
    cout << "[VideoWrite] Waiting for first queueInput data..." << endl;
    while (1)
    {
        waitCount++;
        // 每1秒打印一次状态
        if (waitCount % 1000 == 0)
        {
            cout << "[VideoWrite] Waiting for queueInput... ("
                 << "Size: " << queueInput.size()
                 << ", bTracking: " << bTracking
                 << ", WaitCount: " << waitCount << ")" << endl;
        }

        if (queueInput.size() > 0)
        {
            cout << "[VideoWrite] Received first frame. Initializing VideoWriter..." << endl;
            cout << "[VideoWrite] Video info: "
                 << video_probs.Video_width << "x" << video_probs.Video_height
                 << ", FPS: " << video_probs.Fps
                 << ", FourCC: " << video_probs.Video_fourcc << endl;

            vid_writer = cv::VideoWriter(save_path, video_probs.Video_fourcc, video_probs.Fps,
                                         cv::Size(video_probs.Video_width, video_probs.Video_height));

            // 检查VideoWriter是否成功初始化
            if (!vid_writer.isOpened())
            {
                cerr << "[VideoWrite] ERROR! Failed to open VideoWriter for: " << save_path << endl;
                cerr << "[VideoWrite] Please check: " << endl;
                cerr << "  1. Output path permissions" << endl;
                cerr << "  2. Valid FourCC code (" << video_probs.Video_fourcc << ")" << endl;
                cerr << "  3. Valid dimensions: " << video_probs.Video_width << "x" << video_probs.Video_height << endl;
                exit(EXIT_FAILURE);
            }
            else
            {
                cout << "[VideoWrite] VideoWriter successfully initialized: " << save_path << endl;
            }
            break;
        }

        // 每1000次等待休眠1秒（约1000×1ms）
        usleep(1000);
    }

    int frameCount = 0;
    cout << "[VideoWrite] Starting video writing loop..." << endl;
    while (1)
    {
        // 处理队列中的帧
        if (queueOutput.size() > 0)
        {
            mtxQueueOutput.lock();
            imageout_idx res_pair = queueOutput.front();
            queueOutput.pop();
            mtxQueueOutput.unlock();

            frameCount++;
            if (frameCount % 10 == 0)
            {
                cout << "[VideoWrite] Writing frame " << res_pair.dets.id
                     << " (Total: " << frameCount << ")" << endl;
            }

            draw_image(res_pair.img, res_pair.dets);
            vid_writer.write(res_pair.img);
        }
        // 检查结束条件
        else if (!bTracking)
        {
            cout << "[VideoWrite] bTracking=false detected. Checking if queue is empty..." << endl;

            // 确保队列真正为空
            usleep(100000); // 额外等待100ms以防有数据在传输中
            if (queueOutput.size() == 0)
            {
                cout << "[VideoWrite] Final queue size: " << queueOutput.size()
                     << ". Releasing VideoWriter." << endl;
                vid_writer.release();
                break;
            }
            else
            {
                cout << "[VideoWrite] WARNING: bTracking=false but queue still has "
                     << queueOutput.size() << " frames!" << endl;
            }
        }
        // 添加空队列时的诊断信息
        else
        {
            static int emptyCount = 0;
            emptyCount++;

            // 每10秒报告一次空队列状态
            if (emptyCount % 10000 == 0)
            {
                cout << "[VideoWrite] Queue empty - Waiting for frames... ("
                     << "EmptyCount: " << emptyCount
                     << ", bTracking: " << bTracking
                     << ", QueueSize: " << queueOutput.size() << ")" << endl;
            }
            usleep(1000); // 避免100% CPU占用
        }
    }
    cout << "[VideoWrite] Process completed. Wrote " << frameCount << " frames." << endl;
}

// 写视频
void videoWrite1(const char *save_path, int cpuid)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpuid, &mask);

    if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0)
        cerr << "set thread affinity failed" << endl;

    printf("Bind videoWrite process to CPU %d\n", cpuid);

    cv::VideoWriter vid_writer;
    while (1)
    {
        // cout << "checkpoint! " << queueInput.size() << "\n";
        if (queueInput.size() > 0)
        {
            // cout << video_probs.Video_width << " " << video_probs.Video_height << endl;
            vid_writer = cv::VideoWriter(save_path, video_probs.Video_fourcc, video_probs.Fps,
                                         cv::Size(video_probs.Video_width, video_probs.Video_height));
            break;
        }
    }

    while (1)
    {
        // if (queueOutput.size()) cout << "checkpoint in VideoWriter: " << queueOutput.size() << "\n";
        // queueOutput 就尝试写
        if (queueOutput.size() > 0)
        {
            mtxQueueOutput.lock();
            imageout_idx res_pair = queueOutput.front();
            queueOutput.pop();
            mtxQueueOutput.unlock();
            draw_image(res_pair.img, res_pair.dets);
            vid_writer.write(res_pair.img); // Save-video
        }
        // 最后一帧检测/追踪结束 bWriting置为false 此时如果queueOutput仍存在元素 继续写
        else if (!bTracking)
        {
            vid_writer.release();
            break;
        }
    }
    cout << "VideoWrite is over." << endl;
}

/*---------------------------------------------------------
	绘制预测框
----------------------------------------------------------*/
string labels[2] = {"person", "vehicle"};
cv::Scalar colorArray[2] = {
    cv::Scalar(139, 0, 0, 255),
    cv::Scalar(139, 0, 139, 255),
};
int draw_image(cv::Mat &img, detect_result_group_t detect_result_group)
{
    char text[256];
    for (auto det_result : detect_result_group.results)
    {
        // sprintf(text, "%s %.1f%%", det_result.name, det_result.confidence * 100);
        sprintf(text, "ID:%d", (int)det_result.trackID);
        int x1 = det_result.x1;
        int y1 = det_result.y1 / IMG_WIDTH * IMG_HEIGHT;
        int x2 = det_result.x2;
        int y2 = det_result.y2 / IMG_WIDTH * IMG_HEIGHT;
        int class_id = det_result.classID;
        rectangle(img, cv::Point(x1, y1), cv::Point(x2, y2), colorArray[class_id % 10], 3);
        putText(img, text, cv::Point(x1, y1 - 12), 1, 2, cv::Scalar(0, 255, 0, 255));
    }
    // imwrite("./display.jpg", img);
    return 0;
}