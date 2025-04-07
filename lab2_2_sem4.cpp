#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <semaphore>
#include <atomic>
#include <random>
#include <chrono>
#include <locale>
#include <unordered_set>

const int num_cameras = 6;
const int num_processors = 6;
const int initial_accelerators = 3;
const int total_frames = 20;

std::atomic<int> available_accelerators(initial_accelerators);
std::atomic<bool> system_running(true);
std::atomic<int> finished_cameras(0);
std::atomic<bool> all_accelerators_broken(false);

std::unordered_set<int> broken_accelerators;
std::mutex broken_acc_mutex;

std::counting_semaphore<initial_accelerators> accelerator_sem(initial_accelerators);

std::mutex queue_mutex;
std::mutex acc_mutex;
int next_accelerator = 0;

struct VideoFrame {
    int camera_id;
    int frame_number;
    int priority;
    bool processed;

    bool operator<(const VideoFrame& other) const {
        return priority < other.priority;
    }
};

std::priority_queue<VideoFrame> frame_queue;
std::random_device rd;
std::mt19937 gen(rd());

int random_int(int min, int max) {
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(gen);
}

void process_frame(VideoFrame frame) {
    if (all_accelerators_broken) {
        std::cout << "ОШИБКА: Все ускорители вышли из строя! Кадр " << frame.frame_number << " не может быть обработан." << std::endl;
        return;
    }

    if (!accelerator_sem.try_acquire_for(std::chrono::milliseconds(10000))) {
        std::cout << "Ошибка: нет доступных ускорителей для кадра " << frame.frame_number << std::endl;
        return;
    }

    int accelerator_id = -1;
    {
        std::lock_guard<std::mutex> lock(acc_mutex);
        do {
            accelerator_id = next_accelerator++;
            if (next_accelerator >= initial_accelerators) {
                next_accelerator = 0;
            }
        } while ([&]() {
            std::lock_guard<std::mutex> lock(broken_acc_mutex);
            return broken_accelerators.count(accelerator_id) > 0;
            }());
    }

    std::cout << "Обработка: Камера " << frame.camera_id << ", кадр " << frame.frame_number << " (приоритет " << frame.priority << ") на ускорителе " << accelerator_id << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(
        frame.priority == 2 ? 100 : 200));

    if (random_int(1, 5000) == 1) {
        bool all_broken = false;
        {
            std::lock_guard<std::mutex> lock(broken_acc_mutex);
            if (broken_accelerators.insert(accelerator_id).second) {
                available_accelerators--;
                std::cout << "Ускоритель " << accelerator_id << " вышел из строя!" << std::endl;

                if (broken_accelerators.size() >= initial_accelerators) {
                    all_broken = true;
                    all_accelerators_broken = true;
                }
            }
        }

        if (all_broken) {
            std::cout << "Критическая ошибка - все ускорители вышли из строя!" << std::endl;
            system_running = false;
        }

        accelerator_sem.release();
        return;
    }

    std::cout << "Готово: Камера " << frame.camera_id << ", кадр " << frame.frame_number << " обработан" << std::endl;
    accelerator_sem.release();
}

void camera_thread(int camera_id) {
    for (int frame_num = 1; frame_num <= total_frames && system_running; ++frame_num) {
        int priority = (frame_num % 2 == 0) ? 2 : 1;

        VideoFrame frame{ camera_id, frame_num, priority, false };

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            frame_queue.push(frame);
            std::cout << "Камера " << camera_id << ": Кадр " << frame_num << " (приоритет " << priority << ")" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(
            random_int(100, 300)));
    }

    finished_cameras++;
}

void processor_thread() {
    while (system_running) {
        VideoFrame frame_to_process;
        bool has_frame = false;

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (!frame_queue.empty()) {
                frame_to_process = frame_queue.top();
                frame_queue.pop();
                has_frame = true;
            }
            else if (finished_cameras == num_cameras) {
                break;
            }
        }

        if (has_frame) {
            process_frame(frame_to_process);
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

int main() {
    setlocale(LC_ALL, "rus");
    std::vector<std::thread> cameras;
    std::vector<std::thread> processors;

    for (int i = 0; i < num_cameras; ++i) {
        cameras.emplace_back(camera_thread, i + 1);
    }

    for (int i = 0; i < num_processors; ++i) {
        processors.emplace_back(processor_thread);
    }

    for (auto& cam : cameras) {
        cam.join();
    }

    for (auto& proc : processors) {
        proc.join();
    }

    std::cout << "Все кадры обработаны" << std::endl;
    std::cout << "Сломанные ускорители: ";
    for (int id : broken_accelerators) {
        std::cout << id << " ";
    }
    std::cout << "Рабочих ускорителей осталось: " << initial_accelerators - broken_accelerators.size() << std::endl;

    if (all_accelerators_broken) {
        std::cout << "Все ускорители вышли из строя во время работы!" << std::endl;
    }

    return 0;
}