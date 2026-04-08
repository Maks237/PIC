#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <random>
#include <chrono>
#include <string>
#include <csignal>

using namespace std;

int shared_data = 0;
bool data_available = false;

int active_readers = 0;
bool writer_active = false;
int waiting_writers = 0;
int reads_since_last_write = 0;

bool stop_requested = false;

mutex mtx;
mutex cout_mtx;
condition_variable cv;

const int NUM_READERS = 4;
const int NUM_WRITERS = 2;

bool can_reader_read() {
    return !writer_active &&
           data_available &&
           !(waiting_writers > 0 && reads_since_last_write >= 3);
}

bool can_writer_write() {
    return !writer_active &&
           active_readers == 0 &&
           (!data_available || reads_since_last_write >= 3);
}

void safe_print(const string& text) {
    lock_guard<mutex> lock(cout_mtx);
    cout << text << endl;
}

int random_ms(mt19937& gen, int min_ms, int max_ms) {
    uniform_int_distribution<int> dist(min_ms, max_ms);
    return dist(gen);
}

int random_value(mt19937& gen) {
    uniform_int_distribution<int> dist(1, 100);
    return dist(gen);
}

void signal_handler(int) {
    stop_requested = true;
}

void reader(int id) {
    mt19937 gen(random_device{}());

    while (!stop_requested) {
        this_thread::sleep_for(chrono::milliseconds(random_ms(gen, 200, 700)));

        int value;

        {
            unique_lock<mutex> lock(mtx);

            while (!stop_requested && !can_reader_read()) {
                cv.wait_for(lock, chrono::milliseconds(100));
            }

            if (stop_requested) {
                break;
            }

            active_readers++;
            value = shared_data;
        }

        safe_print("Czytelnik " + to_string(id) + " odczytal: " + to_string(value));

        this_thread::sleep_for(chrono::milliseconds(random_ms(gen, 100, 400)));

        {
            unique_lock<mutex> lock(mtx);
            active_readers--;
            reads_since_last_write++;
        }

        cv.notify_all();
    }

    safe_print("Czytelnik " + to_string(id) + " konczy prace.");
}

void writer(int id) {
    mt19937 gen(random_device{}());

    while (!stop_requested) {
        this_thread::sleep_for(chrono::milliseconds(random_ms(gen, 400, 900)));

        int new_value = random_value(gen);

        {
            unique_lock<mutex> lock(mtx);

            waiting_writers++;

            while (!stop_requested && !can_writer_write()) {
                cv.wait_for(lock, chrono::milliseconds(100));
            }

            if (stop_requested) {
                waiting_writers--;
                break;
            }

            waiting_writers--;
            writer_active = true;
        }

        safe_print("\nPisarz " + to_string(id) + " przygotowuje zapis: " + to_string(new_value));

        this_thread::sleep_for(chrono::milliseconds(random_ms(gen, 150, 350)));

        {
            unique_lock<mutex> lock(mtx);

            shared_data = new_value;
            data_available = true;
            reads_since_last_write = 0;
            writer_active = false;
        }

        safe_print("Pisarz " + to_string(id) + " zapisal: " + to_string(new_value));

        cv.notify_all();
    }

    safe_print("Pisarz " + to_string(id) + " konczy prace.");
}

int main() {
    signal(SIGINT, signal_handler);

    vector<thread> readers;
    vector<thread> writers;

    for (int i = 0; i < NUM_READERS; i++) {
        readers.emplace_back(reader, i + 1);
    }

    for (int i = 0; i < NUM_WRITERS; i++) {
        writers.emplace_back(writer, i + 1);
    }

    for (auto& t : writers) {
        t.join();
    }

    for (auto& t : readers) {
        t.join();
    }

    safe_print("Koniec programu.");
    return 0;
}