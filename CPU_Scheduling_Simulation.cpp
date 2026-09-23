/*
 * CPU Scheduling Algorithms Simulation Using MANUALLY IMPLEMENTED Data Structures
 * C++17
 *
 * No STL container data structures are used:
 *   - no vector
 *   - no queue
 *   - no stack
 *   - no priority_queue
 *
 * The project uses custom implementations of:
 *   - Linked List
 *   - Queue
 *   - Priority Queue
 *   - Stack
 *
 * Round Robin quantum is calculated automatically as the average Burst Time.
 * A final performance table compares Average Waiting, Turnaround and Response Time.
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>
#include <stdexcept>
#include <limits>

using namespace std;
using Clock = chrono::steady_clock;

const int MAX_PROCESSES = 100;
const double EPS = 1e-12;

struct Operation {
    double left{};
    char op{};
    double right{};
    double result{};
    string expression;
};

struct Process {
    string id;
    double arrivalTime{};
    double burstTime{};
    int priority{};
    Operation operation;

    double waitingTime{};
    double turnaroundTime{};
    double completionTime{};
    double responseTime{};
    double remainingTime{};
    double firstStartTime{-1.0};
};

// ============================================================
// MANUAL LINKED LIST
// ============================================================
template <typename T>
class LinkedList {
private:
    struct Node {
        T data;
        Node* next;
        Node(const T& d) : data(d), next(nullptr) {}
    };

    Node* head = nullptr;
    Node* tail = nullptr;
    int count = 0;

public:
    ~LinkedList() { clear(); }

    void push_back(const T& value) {
        Node* n = new Node(value);
        if (!head) head = tail = n;
        else {
            tail->next = n;
            tail = n;
        }
        ++count;
    }

    int size() const { return count; }

    bool removeById(const string& id) {
        Node* cur = head;
        Node* prev = nullptr;

        while (cur) {
            if (cur->data.id == id) {
                if (prev) prev->next = cur->next;
                else head = cur->next;

                if (cur == tail) tail = prev;

                delete cur;
                --count;
                return true;
            }
            prev = cur;
            cur = cur->next;
        }
        return false;
    }

    void copyToArray(T arr[], int capacity) const {
        Node* cur = head;
        int i = 0;
        while (cur && i < capacity) {
            arr[i++] = cur->data;
            cur = cur->next;
        }
    }

    void clear() {
        Node* cur = head;
        while (cur) {
            Node* next = cur->next;
            delete cur;
            cur = next;
        }
        head = tail = nullptr;
        count = 0;
    }
};

// ============================================================
// MANUAL QUEUE - circular array
// ============================================================
class IntQueue {
private:
    int data[MAX_PROCESSES * 4];
    int frontIndex = 0;
    int rearIndex = 0;
    int count = 0;

public:
    bool empty() const { return count == 0; }

    void push(int value) {
        if (count >= MAX_PROCESSES * 4)
            throw runtime_error("Queue overflow.");
        data[rearIndex] = value;
        rearIndex = (rearIndex + 1) % (MAX_PROCESSES * 4);
        ++count;
    }

    int front() const {
        if (empty()) throw runtime_error("Queue is empty.");
        return data[frontIndex];
    }

    void pop() {
        if (empty()) throw runtime_error("Queue is empty.");
        frontIndex = (frontIndex + 1) % (MAX_PROCESSES * 4);
        --count;
    }
};

// ============================================================
// MANUAL PRIORITY QUEUE
// Lower key = higher priority.
// Implemented as a binary min-heap.
// ============================================================
struct HeapItem {
    double key;
    int index;
    int tie;
};

class MinHeap {
private:
    HeapItem heap[MAX_PROCESSES];
    int count = 0;

    bool lessItem(const HeapItem& a, const HeapItem& b) const {
        if (a.key != b.key) return a.key < b.key;
        return a.tie < b.tie;
    }

    void swapItems(HeapItem& a, HeapItem& b) {
        HeapItem temp = a;
        a = b;
        b = temp;
    }

public:
    bool empty() const { return count == 0; }

    void push(double key, int index, int tie) {
        if (count >= MAX_PROCESSES)
            throw runtime_error("Priority queue overflow.");

        int pos = count++;
        heap[pos] = {key, index, tie};

        while (pos > 0) {
            int parent = (pos - 1) / 2;
            if (!lessItem(heap[pos], heap[parent])) break;
            swapItems(heap[pos], heap[parent]);
            pos = parent;
        }
    }

    HeapItem top() const {
        if (empty()) throw runtime_error("Priority queue is empty.");
        return heap[0];
    }

    void pop() {
        if (empty()) throw runtime_error("Priority queue is empty.");
        --count;
        if (count == 0) return;

        heap[0] = heap[count];
        int pos = 0;

        while (true) {
            int left = pos * 2 + 1;
            int right = pos * 2 + 2;
            int smallest = pos;

            if (left < count && lessItem(heap[left], heap[smallest]))
                smallest = left;
            if (right < count && lessItem(heap[right], heap[smallest]))
                smallest = right;

            if (smallest == pos) break;
            swapItems(heap[pos], heap[smallest]);
            pos = smallest;
        }
    }
};

// ============================================================
// MANUAL STACK
// ============================================================
class StringStack {
private:
    string data[MAX_PROCESSES];
    int topIndex = -1;

public:
    bool empty() const { return topIndex < 0; }

    void push(const string& value) {
        if (topIndex >= MAX_PROCESSES - 1)
            throw runtime_error("Stack overflow.");
        data[++topIndex] = value;
    }

    string top() const {
        if (empty()) throw runtime_error("Stack is empty.");
        return data[topIndex];
    }

    void pop() {
        if (empty()) throw runtime_error("Stack is empty.");
        --topIndex;
    }
};

// ============================================================
// Arithmetic evaluation and measurement
// ============================================================
double evaluate(const Operation& x) {
    switch (x.op) {
        case '+': return x.left + x.right;
        case '-': return x.left - x.right;
        case '*': return x.left * x.right;
        case '/':
            if (x.right == 0.0)
                throw runtime_error("Division by zero is not allowed.");
            return x.left / x.right;
        default:
            throw runtime_error("Unsupported operator.");
    }
}

// Manual parser: no stringstream or other parsing helper is used.
// It reads the characters itself and builds the left number, operator, and right number.
Operation parseExpression(const string& text) {
    Operation x;
    x.expression = text;

    int i = 0;
    const int len = static_cast<int>(text.size());

    // Skip spaces manually.
    while (i < len && (text[i] == ' ' || text[i] == '\t'))
        ++i;

    // Read a number manually: optional sign + digits + optional decimal part.
    auto readNumber = [&](double& value) -> bool {
        while (i < len && (text[i] == ' ' || text[i] == '\t'))
            ++i;

        int sign = 1;
        if (i < len && (text[i] == '+' || text[i] == '-')) {
            if (text[i] == '-') sign = -1;
            ++i;
        }

        bool hasDigit = false;
        double number = 0.0;

        while (i < len && text[i] >= '0' && text[i] <= '9') {
            hasDigit = true;
            number = number * 10.0 + (text[i] - '0');
            ++i;
        }

        if (i < len && text[i] == '.') {
            ++i;
            double place = 0.1;
            while (i < len && text[i] >= '0' && text[i] <= '9') {
                hasDigit = true;
                number += (text[i] - '0') * place;
                place *= 0.1;
                ++i;
            }
        }

        if (!hasDigit)
            return false;

        value = sign * number;
        return true;
    };

    // Left operand.
    if (!readNumber(x.left))
        throw runtime_error("Use the format: number operator number (e.g. 10 + 20)");

    while (i < len && (text[i] == ' ' || text[i] == '\t'))
        ++i;

    // Operator.
    if (i >= len || (text[i] != '+' && text[i] != '-' &&
                     text[i] != '*' && text[i] != '/'))
        throw runtime_error("Operator must be +, -, * or /.");

    x.op = text[i];
    ++i;

    // Right operand.
    if (!readNumber(x.right))
        throw runtime_error("Use the format: number operator number (e.g. 10 + 20)");

    // Only spaces are allowed after the right operand.
    while (i < len && (text[i] == ' ' || text[i] == '\t'))
        ++i;

    if (i != len)
        throw runtime_error("Only simple binary operations are supported.");

    if (x.op == '/' && x.right == 0.0)
        throw runtime_error("Division by zero is not allowed.");

    x.result = evaluate(x);
    return x;
}

double measureCpuBurstMs(const Operation& op, long long repetitions) {
    volatile double sink = 0.0;

    auto start = Clock::now();
    for (long long i = 0; i < repetitions; ++i)
        sink = evaluate(op);
    auto end = Clock::now();

    (void)sink;
    double totalNs = chrono::duration<double, nano>(end - start).count();
    double perOperationNs = totalNs / static_cast<double>(repetitions);
    return perOperationNs / 1'000'000.0;
}

// ============================================================
// Helpers
// ============================================================
void resetMetrics(Process p[], int n) {
    for (int i = 0; i < n; ++i) {
        p[i].waitingTime = 0.0;
        p[i].turnaroundTime = 0.0;
        p[i].completionTime = 0.0;
        p[i].responseTime = 0.0;
        p[i].remainingTime = p[i].burstTime;
        p[i].firstStartTime = -1.0;
    }
}

void printExecutionOrder(const string order[], int orderCount) {
    if (orderCount == 0) {
        cout << "Execution Order: (none)\n";
        return;
    }

    cout << "Execution Order: ";
    for (int i = 0; i < orderCount; ++i) {
        if (i) cout << " -> ";
        cout << order[i];
    }
    cout << '\n';
}

void printResults(const string& name, const Process p[], int n,
                  const string order[], int orderCount) {
    double avgWait = 0.0, avgTurn = 0.0, avgResponse = 0.0;

    cout << "\n============================================================\n";
    cout << name << '\n';
    cout << "============================================================\n";
    printExecutionOrder(order, orderCount);

    cout << fixed << setprecision(6);
    cout << left
         << setw(8) << "ID"
         << setw(16) << "Burst(ms)"
         << setw(16) << "Waiting(ms)"
         << setw(18) << "Turnaround(ms)"
         << setw(18) << "Response(ms)"
         << setw(18) << "Completion(ms)" << '\n';

    for (int i = 0; i < n; ++i) {
        cout << left
             << setw(8) << p[i].id
             << setw(16) << p[i].burstTime
             << setw(16) << p[i].waitingTime
             << setw(18) << p[i].turnaroundTime
             << setw(18) << p[i].responseTime
             << setw(18) << p[i].completionTime << '\n';

        avgWait += p[i].waitingTime;
        avgTurn += p[i].turnaroundTime;
        avgResponse += p[i].responseTime;
    }

    avgWait /= n;
    avgTurn /= n;
    avgResponse /= n;

    cout << "\nAverage Waiting Time    : " << avgWait << " ms\n";
    cout << "Average Turnaround Time : " << avgTurn << " ms\n";
    cout << "Average Response Time   : " << avgResponse << " ms\n";
}

// ============================================================
// FCFS - manual Queue
// ============================================================
void fcfs(const Process input[], int n) {
    Process p[MAX_PROCESSES];
    for (int i = 0; i < n; ++i) p[i] = input[i];
    resetMetrics(p, n);

    IntQueue readyQueue;
    bool added[MAX_PROCESSES] = {};
    string order[MAX_PROCESSES];
    int orderCount = 0;
    double time = 0.0;
    int completed = 0;

    while (completed < n) {
        for (int i = 0; i < n; ++i) {
            if (!added[i] && p[i].arrivalTime <= time) {
                readyQueue.push(i);
                added[i] = true;
            }
        }

        if (readyQueue.empty()) {
            double nextArrival = numeric_limits<double>::max();
            for (int i = 0; i < n; ++i)
                if (!added[i] && p[i].arrivalTime < nextArrival)
                    nextArrival = p[i].arrivalTime;
            time = nextArrival;
            continue;
        }

        int idx = readyQueue.front();
        readyQueue.pop();

        p[idx].firstStartTime = time;
        p[idx].responseTime = time - p[idx].arrivalTime;
        if (p[idx].responseTime < 0) p[idx].responseTime = 0;
        p[idx].waitingTime = p[idx].responseTime;

        time += p[idx].burstTime;
        p[idx].completionTime = time;
        p[idx].turnaroundTime = time - p[idx].arrivalTime;
        order[orderCount++] = p[idx].id;
        ++completed;
    }

    printResults("First Come First Serve (FCFS)", p, n, order, orderCount);
}

// ============================================================
// SJF - manual Priority Queue (min-heap)
// ============================================================
void sjf(const Process input[], int n) {
    Process p[MAX_PROCESSES];
    for (int i = 0; i < n; ++i) p[i] = input[i];
    resetMetrics(p, n);

    MinHeap readyQueue;
    bool added[MAX_PROCESSES] = {};
    string order[MAX_PROCESSES];
    int orderCount = 0;
    double time = 0.0;
    int completed = 0;

    while (completed < n) {
        for (int i = 0; i < n; ++i) {
            if (!added[i] && p[i].arrivalTime <= time) {
                readyQueue.push(p[i].burstTime, i, i);
                added[i] = true;
            }
        }

        if (readyQueue.empty()) {
            double nextArrival = numeric_limits<double>::max();
            for (int i = 0; i < n; ++i)
                if (!added[i] && p[i].arrivalTime < nextArrival)
                    nextArrival = p[i].arrivalTime;
            time = nextArrival;
            continue;
        }

        int idx = readyQueue.top().index;
        readyQueue.pop();

        p[idx].firstStartTime = time;
        p[idx].responseTime = time - p[idx].arrivalTime;
        if (p[idx].responseTime < 0) p[idx].responseTime = 0;
        p[idx].waitingTime = p[idx].responseTime;

        time += p[idx].burstTime;
        p[idx].completionTime = time;
        p[idx].turnaroundTime = time - p[idx].arrivalTime;
        order[orderCount++] = p[idx].id;
        ++completed;
    }

    printResults("Shortest Job First (SJF) - Non-Preemptive", p, n, order, orderCount);
}

// ============================================================
// Priority Scheduling - manual Priority Queue (min-heap)
// ============================================================
void priorityScheduling(const Process input[], int n) {
    Process p[MAX_PROCESSES];
    for (int i = 0; i < n; ++i) p[i] = input[i];
    resetMetrics(p, n);

    MinHeap readyQueue;
    bool added[MAX_PROCESSES] = {};
    string order[MAX_PROCESSES];
    int orderCount = 0;
    double time = 0.0;
    int completed = 0;

    while (completed < n) {
        for (int i = 0; i < n; ++i) {
            if (!added[i] && p[i].arrivalTime <= time) {
                readyQueue.push(static_cast<double>(p[i].priority), i, i);
                added[i] = true;
            }
        }

        if (readyQueue.empty()) {
            double nextArrival = numeric_limits<double>::max();
            for (int i = 0; i < n; ++i)
                if (!added[i] && p[i].arrivalTime < nextArrival)
                    nextArrival = p[i].arrivalTime;
            time = nextArrival;
            continue;
        }

        int idx = readyQueue.top().index;
        readyQueue.pop();

        p[idx].firstStartTime = time;
        p[idx].responseTime = time - p[idx].arrivalTime;
        if (p[idx].responseTime < 0) p[idx].responseTime = 0;
        p[idx].waitingTime = p[idx].responseTime;

        time += p[idx].burstTime;
        p[idx].completionTime = time;
        p[idx].turnaroundTime = time - p[idx].arrivalTime;
        order[orderCount++] = p[idx].id;
        ++completed;
    }

    printResults("Priority Scheduling (Non-Preemptive)", p, n, order, orderCount);
}

// ============================================================
// Round Robin - manual Queue
// ============================================================
void roundRobin(const Process input[], int n, double quantum) {
    Process p[MAX_PROCESSES];
    for (int i = 0; i < n; ++i) p[i] = input[i];
    resetMetrics(p, n);

    IntQueue readyQueue;
    bool added[MAX_PROCESSES] = {};
    string order[MAX_PROCESSES * 100];
    int orderCount = 0;
    double time = 0.0;
    int completed = 0;

    while (completed < n) {
        for (int i = 0; i < n; ++i) {
            if (!added[i] && p[i].arrivalTime <= time) {
                readyQueue.push(i);
                added[i] = true;
            }
        }

        if (readyQueue.empty()) {
            double nextArrival = numeric_limits<double>::max();
            for (int i = 0; i < n; ++i)
                if (!added[i] && p[i].arrivalTime < nextArrival)
                    nextArrival = p[i].arrivalTime;
            time = nextArrival;
            continue;
        }

        int idx = readyQueue.front();
        readyQueue.pop();

        if (p[idx].firstStartTime < 0.0) {
            p[idx].firstStartTime = time;
            p[idx].responseTime = time - p[idx].arrivalTime;
            if (p[idx].responseTime < 0) p[idx].responseTime = 0;
        }

        double runTime = (p[idx].remainingTime < quantum)
                       ? p[idx].remainingTime : quantum;

        p[idx].remainingTime -= runTime;
        time += runTime;

        if (orderCount < MAX_PROCESSES * 100)
            order[orderCount++] = p[idx].id;

        for (int i = 0; i < n; ++i) {
            if (!added[i] && p[i].arrivalTime <= time) {
                readyQueue.push(i);
                added[i] = true;
            }
        }

        if (p[idx].remainingTime > EPS) {
            readyQueue.push(idx);
        } else {
            p[idx].remainingTime = 0.0;
            p[idx].completionTime = time;
            p[idx].turnaroundTime = time - p[idx].arrivalTime;
            p[idx].waitingTime = p[idx].turnaroundTime - p[idx].burstTime;
            if (p[idx].waitingTime < 0.0) p[idx].waitingTime = 0.0;
            ++completed;
        }
    }

    printResults("Round Robin (RR)", p, n, order, orderCount);
}

// ============================================================
// Manual Stack demonstration + table
// ============================================================
void demonstrateStack(const Process processes[], int n,
                      const string executionOrder[], int orderCount) {
    StringStack history;

    for (int i = 0; i < orderCount; ++i)
        history.push(executionOrder[i]);

    cout << "\n============================================================\n";
    cout << "STACK DEMONSTRATION (MANUAL LIFO)\n";
    cout << "============================================================\n";
    cout << "Push order: ";
    for (int i = 0; i < orderCount; ++i) {
        if (i) cout << " -> ";
        cout << executionOrder[i];
    }
    cout << "\nPop order : ";
    StringStack preview = history;
    bool firstPreview = true;
    while (!preview.empty()) {
        if (!firstPreview) cout << " -> ";
        cout << preview.top();
        preview.pop();
        firstPreview = false;
    }
    cout << "\n\n";
    cout << left
         << setw(8) << "Pop#"
         << setw(8) << "ID"
         << setw(24) << "Operation"
         << setw(16) << "Result"
         << setw(16) << "Burst(ms)"
         << setw(18) << "Cumulative(ms)" << '\n';

    // Rebuild stack because the Pop table is the actual LIFO operation.
    StringStack stack;
    for (int i = 0; i < orderCount; ++i)
        stack.push(executionOrder[i]);

    double cumulative = 0.0;
    int popNumber = 1;

    cout << fixed << setprecision(6);
    while (!stack.empty()) {
        string id = stack.top();
        stack.pop();

        int idx = -1;
        for (int i = 0; i < n; ++i) {
            if (processes[i].id == id) {
                idx = i;
                break;
            }
        }

        if (idx >= 0) {
            cumulative += processes[idx].burstTime;
            cout << left
                 << setw(8) << popNumber
                 << setw(8) << processes[idx].id
                 << setw(24) << processes[idx].operation.expression
                 << setw(16) << processes[idx].operation.result
                 << setw(16) << processes[idx].burstTime
                 << setw(18) << cumulative << '\n';
        }
        ++popNumber;
    }
}

// ============================================================
// Performance comparison table
// ============================================================
struct PerformanceRow {
    string algorithm;
    double avgWaiting;
    double avgTurnaround;
    double avgResponse;
};

void calculateSummary(const Process input[], int n,
                      const string& algorithm, double quantum,
                      PerformanceRow& row) {
    // This helper is not used for scheduling; it exists only for clarity.
    // The actual summary is calculated by run-and-collect functions below.
    (void)input; (void)n; (void)algorithm; (void)quantum; (void)row;
}

void runFCFSForSummary(const Process input[], int n, PerformanceRow& row) {
    Process p[MAX_PROCESSES];
    for (int i = 0; i < n; ++i) p[i] = input[i];
    resetMetrics(p, n);

    IntQueue q;
    bool added[MAX_PROCESSES] = {};
    double time = 0.0;
    int completed = 0;
    double sw = 0, st = 0, sr = 0;

    while (completed < n) {
        for (int i = 0; i < n; ++i)
            if (!added[i] && p[i].arrivalTime <= time) { q.push(i); added[i] = true; }
        if (q.empty()) {
            double next = numeric_limits<double>::max();
            for (int i = 0; i < n; ++i) if (!added[i] && p[i].arrivalTime < next) next = p[i].arrivalTime;
            time = next; continue;
        }
        int idx = q.front(); q.pop();
        p[idx].responseTime = time - p[idx].arrivalTime;
        if (p[idx].responseTime < 0) p[idx].responseTime = 0;
        p[idx].waitingTime = p[idx].responseTime;
        time += p[idx].burstTime;
        p[idx].completionTime = time;
        p[idx].turnaroundTime = time - p[idx].arrivalTime;
        sw += p[idx].waitingTime; st += p[idx].turnaroundTime; sr += p[idx].responseTime;
        ++completed;
    }
    row = {"FCFS", sw / n, st / n, sr / n};
}

void runSJFForSummary(const Process input[], int n, PerformanceRow& row) {
    Process p[MAX_PROCESSES];
    for (int i = 0; i < n; ++i) p[i] = input[i];
    resetMetrics(p, n);
    MinHeap q;
    bool added[MAX_PROCESSES] = {};
    double time = 0.0; int completed = 0; double sw=0,st=0,sr=0;
    while (completed<n) {
        for(int i=0;i<n;++i) if(!added[i]&&p[i].arrivalTime<=time){q.push(p[i].burstTime,i,i);added[i]=true;}
        if(q.empty()){double next=numeric_limits<double>::max();for(int i=0;i<n;++i)if(!added[i]&&p[i].arrivalTime<next)next=p[i].arrivalTime;time=next;continue;}
        int idx=q.top().index;q.pop();p[idx].responseTime=time-p[idx].arrivalTime;if(p[idx].responseTime<0)p[idx].responseTime=0;p[idx].waitingTime=p[idx].responseTime;time+=p[idx].burstTime;p[idx].completionTime=time;p[idx].turnaroundTime=time-p[idx].arrivalTime;sw+=p[idx].waitingTime;st+=p[idx].turnaroundTime;sr+=p[idx].responseTime;++completed;
    }
    row={"SJF (Non-Preemptive)",sw/n,st/n,sr/n};
}

void runPriorityForSummary(const Process input[], int n, PerformanceRow& row) {
    Process p[MAX_PROCESSES];for(int i=0;i<n;++i)p[i]=input[i];resetMetrics(p,n);MinHeap q;bool added[MAX_PROCESSES]={};double time=0;int completed=0;double sw=0,st=0,sr=0;
    while(completed<n){for(int i=0;i<n;++i)if(!added[i]&&p[i].arrivalTime<=time){q.push((double)p[i].priority,i,i);added[i]=true;}if(q.empty()){double next=numeric_limits<double>::max();for(int i=0;i<n;++i)if(!added[i]&&p[i].arrivalTime<next)next=p[i].arrivalTime;time=next;continue;}int idx=q.top().index;q.pop();p[idx].responseTime=time-p[idx].arrivalTime;if(p[idx].responseTime<0)p[idx].responseTime=0;p[idx].waitingTime=p[idx].responseTime;time+=p[idx].burstTime;p[idx].completionTime=time;p[idx].turnaroundTime=time-p[idx].arrivalTime;sw+=p[idx].waitingTime;st+=p[idx].turnaroundTime;sr+=p[idx].responseTime;++completed;}
    row={"Priority Scheduling",sw/n,st/n,sr/n};
}

void runRRForSummary(const Process input[], int n, double quantum, PerformanceRow& row) {
    Process p[MAX_PROCESSES];for(int i=0;i<n;++i)p[i]=input[i];resetMetrics(p,n);IntQueue q;bool added[MAX_PROCESSES]={};double time=0;int completed=0;double sw=0,st=0,sr=0;
    while(completed<n){for(int i=0;i<n;++i)if(!added[i]&&p[i].arrivalTime<=time){q.push(i);added[i]=true;}if(q.empty()){double next=numeric_limits<double>::max();for(int i=0;i<n;++i)if(!added[i]&&p[i].arrivalTime<next)next=p[i].arrivalTime;time=next;continue;}int idx=q.front();q.pop();if(p[idx].firstStartTime<0){p[idx].firstStartTime=time;p[idx].responseTime=time-p[idx].arrivalTime;if(p[idx].responseTime<0)p[idx].responseTime=0;}double run=p[idx].remainingTime<quantum?p[idx].remainingTime:quantum;p[idx].remainingTime-=run;time+=run;for(int i=0;i<n;++i)if(!added[i]&&p[i].arrivalTime<=time){q.push(i);added[i]=true;}if(p[idx].remainingTime>EPS)q.push(idx);else{p[idx].remainingTime=0;p[idx].completionTime=time;p[idx].turnaroundTime=time-p[idx].arrivalTime;p[idx].waitingTime=p[idx].turnaroundTime-p[idx].burstTime;sw+=p[idx].waitingTime;st+=p[idx].turnaroundTime;sr+=p[idx].responseTime;++completed;}}
    row={"Round Robin",sw/n,st/n,sr/n};
}

void printPerformanceComparison(const PerformanceRow rows[], int count, double quantum) {
    cout << "\n\n============================================================\n";
    cout << "PERFORMANCE COMPARISON\n";
    cout << "============================================================\n";
    cout << "Algorithm" << setw(30) << "" << "Avg Waiting  Avg Turnaround  Avg Response\n";
    cout << "------------------------------------------------------------\n";
    cout << fixed << setprecision(6);
    for (int i = 0; i < count; ++i) {
        cout << left << setw(34) << rows[i].algorithm
             << setw(14) << rows[i].avgWaiting
             << setw(16) << rows[i].avgTurnaround
             << setw(14) << rows[i].avgResponse << '\n';
    }
    cout << "------------------------------------------------------------\n";
    cout << "Automatically calculated Round Robin Quantum = " << quantum << " ms\n";

    int bestWait = 0, bestTurn = 0;
    for (int i = 1; i < count; ++i) {
        if (rows[i].avgWaiting < rows[bestWait].avgWaiting) bestWait = i;
        if (rows[i].avgTurnaround < rows[bestTurn].avgTurnaround) bestTurn = i;
    }
    cout << ">>> Lowest average waiting time: " << rows[bestWait].algorithm << '\n';
    cout << ">>> Lowest average turnaround time: " << rows[bestTurn].algorithm << '\n';
    cout << "(These are measurements for this input workload, not a universal ranking.)\n";
}

// ============================================================
// Input
// ============================================================
int readInt(const string& prompt, int minValue = 0) {
    while (true) {
        cout << prompt;
        int x;
        if (cin >> x && x >= minValue) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return x;
        }
        cout << "Invalid input. Try again.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

double readDouble(const string& prompt, double minValue = 0.0) {
    while (true) {
        cout << prompt;
        double x;
        if (cin >> x && x >= minValue) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return x;
        }
        cout << "Invalid input. Try again.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

int main() {
    cout << "============================================================\n";
    cout << " CPU Scheduling Algorithms Simulation\n";
    cout << " MANUAL DATA STRUCTURES + AUTOMATIC RR QUANTUM\n";
    cout << "============================================================\n\n";

    int n = readInt("Number of processes: ", 1);
    if (n > MAX_PROCESSES) {
        cout << "Maximum supported processes = " << MAX_PROCESSES << "\n";
        return 1;
    }

    long long repetitions = readInt(
        "Repetitions used for CPU-time measurement (e.g. 1000000): ", 1);

    // NO manual Quantum input anymore.
    cout << "Round Robin Quantum will be calculated automatically from the average Burst Time.\n";

    LinkedList<Process> processList;

    for (int i = 1; i <= n; ++i) {
        cout << "\n--- Process P" << i << " ---\n";

        string expr;
        Operation op;
        while (true) {
            cout << "Enter arithmetic operation (e.g. 10 + 20): ";
            getline(cin, expr);
            try {
                op = parseExpression(expr);
                break;
            } catch (const exception& e) {
                cout << "Error: " << e.what() << '\n';
            }
        }

        double arrival = readDouble("Arrival Time (ms): ", 0.0);
        int priority = readInt("Priority (smaller number = higher priority): ", 0);
        double burst = measureCpuBurstMs(op, repetitions);

        Process p;
        p.id = "P" + to_string(i);
        p.arrivalTime = arrival;
        p.burstTime = burst;
        p.priority = priority;
        p.operation = op;
        processList.push_back(p);

        cout << fixed << setprecision(9);
        cout << "Result: " << op.result << '\n';
        cout << "Measured CPU time for one operation: " << burst << " ms\n";
    }

    // Run all scheduling calculations using the current process list.
    auto runAllCalculations = [&](int currentN) {
        Process processes[MAX_PROCESSES];
        processList.copyToArray(processes, MAX_PROCESSES);

        double sumBurst = 0.0;
        for (int i = 0; i < currentN; ++i) sumBurst += processes[i].burstTime;
        double quantum = sumBurst / currentN;
        if (quantum <= EPS) quantum = 0.000001;

        cout << "\n================ INPUT PROCESSES ===========================\n";
        cout << fixed << setprecision(9);
        cout << left
             << setw(8) << "ID"
             << setw(24) << "Operation"
             << setw(16) << "Result"
             << setw(16) << "Arrival(ms)"
             << setw(16) << "Burst(ms)"
             << setw(10) << "Priority" << '\n';

        for (int i = 0; i < currentN; ++i) {
            cout << left
                 << setw(8) << processes[i].id
                 << setw(24) << processes[i].operation.expression
                 << setw(16) << processes[i].operation.result
                 << setw(16) << processes[i].arrivalTime
                 << setw(16) << processes[i].burstTime
                 << setw(10) << processes[i].priority << '\n';
        }

        cout << "\nAverage Burst Time = " << quantum << " ms\n";
        cout << "Automatic Round Robin Quantum = Average Burst Time = "
             << quantum << " ms\n";

        fcfs(processes, currentN);
        sjf(processes, currentN);
        priorityScheduling(processes, currentN);
        roundRobin(processes, currentN, quantum);

        // Stack demonstration uses the current input/execution order.
        string stackOrder[MAX_PROCESSES];
        for (int i = 0; i < currentN; ++i) stackOrder[i] = processes[i].id;
        demonstrateStack(processes, currentN, stackOrder, currentN);

        PerformanceRow rows[4];
        runFCFSForSummary(processes, currentN, rows[0]);
        runSJFForSummary(processes, currentN, rows[1]);
        runPriorityForSummary(processes, currentN, rows[2]);
        runRRForSummary(processes, currentN, quantum, rows[3]);
        printPerformanceComparison(rows, 4, quantum);
    };

    // First calculation: exactly the same workflow as before.
    runAllCalculations(n);

    // Optional deletion AFTER all original results have been displayed.
    cout << "\n============================================================\n";
    cout << " Do you want to delete a process?\n";
    cout << " 1 = Yes\n";
    cout << " 0 = No\n";
    cout << "============================================================\n";

    int deleteChoice = readInt("Enter choice: ", 0);

    if (deleteChoice == 1) {
        string processId;
        cout << "Enter Process ID to delete (e.g. P2): ";
        cin >> processId;

        if (processList.removeById(processId)) {
            --n;
            cout << "\nProcess " << processId << " deleted successfully.\n";

            if (n > 0) {
                cout << "\nRecalculating all scheduling algorithms and tables...\n";
                runAllCalculations(n);
            } else {
                cout << "No processes remain after deletion.\n";
            }
        } else {
            cout << "Process " << processId << " was not found.\n";
            cout << "All previous results remain unchanged.\n";
        }
    } else {
        cout << "No process deleted.\n";
    }

    cout << "\nProject completed.\n";
    cout << "Note: measured CPU time depends on the machine, compiler,\n";
    cout << "background load, and repetition count.\n";
    return 0;
}
