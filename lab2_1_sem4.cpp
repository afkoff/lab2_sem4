#include <iostream>
#include <thread>
#include <semaphore>
#include <queue>
#include <mutex>
#include <random>
#include <chrono>
#include <vector>
#include <atomic>

const int num_cranes = 5;
const int num_trucs = 10;
const int high_queue_threshold = 5;
const int low_trucs_threshold = 3;

std::counting_semaphore<num_cranes+1> cranes(num_cranes);
std::mutex queue_mutex;
std::queue<int> truck_queue;
std::atomic<int> loaded_trucks(0);
std::atomic<int> active_cranes(num_cranes);
std::atomic<bool> emergency_mode(false);

void truck_loading(int truck_id) {

    {
        std::lock_guard<std::mutex> lock(queue_mutex);

        if (loaded_trucks < low_trucs_threshold && !emergency_mode) {
            emergency_mode = true;
            std::cout << "����������� ��������� ����� ��������" << std::endl;
        }
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        truck_queue.push(truck_id);
        std::cout << "�������� " << truck_id << " ������ � ����. ������ �������: " << truck_queue.size() << std::endl;

        if (truck_queue.size() > high_queue_threshold && active_cranes < num_cranes + 1) {
            active_cranes++;
            std::cout << "������� ��������� ����. �������� ������: " << active_cranes << std::endl;
            cranes.release();
        }
    }

    cranes.acquire();

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        truck_queue.pop();
        std::cout << "�������� " << truck_id << " ����� ��������. �������� � �������: " << truck_queue.size() << std::endl;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> loading_time(3, 6);

    int load_time = loading_time(gen);
    if (emergency_mode) {
        load_time = std::max(1, load_time / 2);
        std::cout << "��������� �����: �������� " << truck_id << " ����������� �� " << load_time << " ���.\n";
    }
    else {
        std::cout << "�������� " << truck_id << " ����� ����������� " << load_time << " ���.\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(load_time));

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        loaded_trucks++;
        std::cout << "�������� " << truck_id << " ��������. ����� ���������: " << loaded_trucks << std::endl;

        if (loaded_trucks >= low_trucs_threshold && emergency_mode) {
            emergency_mode = false;
            std::cout << "��������� ����� ��������." << std::endl;
        }
    }

    cranes.release();

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (truck_queue.size() <= high_queue_threshold - 2 && active_cranes > num_cranes) { // high_queue_threshold - 2 ������ ��� ������� �������������, ����� ��������� ���� ��������� �� ��������� � �� ���������� ��� ������� ��������
            if (cranes.try_acquire_for(std::chrono::milliseconds(100))) {
                active_cranes--;
                std::cout << "�������� ��������� ����. �������� ������: " << active_cranes << std::endl;
            }
        }
    }
}

int main() {
    setlocale(LC_ALL, "rus");

    std::vector<std::thread> trucks;

    std::cout << "��������� ����� ��������. ������: " << num_cranes << ", ����������: " << num_trucs << std::endl;

    for (int i = 1; i <= num_trucs; ++i) {
        trucks.emplace_back(truck_loading, i);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    for (auto& truck : trucks) {
        truck.join();
    }

    std::cout << "��� ��������� ���������. ��������� ���������." << std::endl;
    return 0;
}
