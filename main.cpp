#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>


using FunctionType = double (*)(int);

// Функція для обчислення x * x * 1.5
double function_g(int x) {
    return x * x * 1.5;
}

// Функція для обчислення квадратного кореня з x
double function_h(int x) {
    return std::sqrt(x);
}

// Функція для обчислення куба x
double function_f(int x) {
    return x * x * x;
}

// Глобальні змінні для обробки сигналів
std::map<pid_t, std::string> activeProcesses;
bool cancelFlag = false;
std::map<std::string, pid_t> components; // Компоненти групи
std::map<std::string, int> pipes; // Pipe для кожного компонента
std::map<std::string, double> results;  // Збереження результатів

std::string currentGroup = "";  

// Обробник сигналу для ручного скасування
void signalHandler(int signum) {
    std::cerr << "\nCancellation signal received (Ctrl+C).\n";
    cancelFlag = true;

    // Завершуємо всі активні процеси
    for (const auto& [pid, name] : activeProcesses) {
        kill(pid, SIGKILL);
        std::cerr << "Terminated component: " << name << " (PID: " << pid << ").\n";
    }
    exit(1);
}

// Менеджер групи
class Manager {
public:
    Manager() {
        signal(SIGINT, signalHandler);
    }

    // Створити групу
    void createGroup(const std::string& name) {
        if (currentGroup.empty()) {
            currentGroup = name;
            std::cout << "Group " << name << " created.\n";
        } else {
            std::cerr << "A group already exists: " << currentGroup << "\n";
        }
    }

    // Додати новий компонент
    void addComponent(const std::string& name, FunctionType func, int arg) {
        if (currentGroup.empty()) {
            std::cerr << "No group created yet. Please create a group first.\n";
            return;
        }

        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe");
            return;
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            return;
        }

        if (pid == 0) { // Дочірній процес
            close(pipefd[0]); // Закриваємо читання
            double result = func(arg); // Виконуємо обчислення
            write(pipefd[1], &result, sizeof(result)); // Надсилаємо результат
            close(pipefd[1]);
            exit(0); // Завершення
        } else { // Батьківський процес
            close(pipefd[1]); // Закриваємо запис
            components[name] = pid; // Зберігаємо PID
            pipes[name] = pipefd[0]; // Зберігаємо pipe
        }
    }

    // Запустити всі компоненти та отримати результати
    void run() {
        if (currentGroup.empty()) {
            std::cerr << "No group created yet. Please create a group first.\n";
            return;
        }

        std::cout << "Running group " << currentGroup << "...\n";

        for (const auto& [name, pid] : components) {
            int status;
            waitpid(pid, &status, 0); // Чекаємо завершення процесу

            if (WIFEXITED(status)) {
                double result;
                read(pipes[name], &result, sizeof(result)); // Отримуємо результат
                results[name] = result;  // Зберігаємо результат
                std::cout << "Component " << name << " finished, result: " << result << '\n';
            } else {
                std::cerr << "Component " << name << " failed.\n";
            }

            close(pipes[name]); // Закриваємо pipe
        }
        components.clear();
        pipes.clear();
    }

    // Вивести статуси компонентів
    void status() const {
        if (currentGroup.empty()) {
            std::cerr << "No group created yet. Please create a group first.\n";
            return;
        }

        for (const auto& [name, pid] : components) {
            std::cout << "Component " << name << " running (PID: " << pid << ")\n";
        }
    }

    // Перегляд результатів
    void summary() const {
        if (currentGroup.empty()) {
            std::cerr << "No group created yet. Please create a group first.\n";
            return;
        }

        for (const auto& [name, result] : results) {
            std::cout << "Component " << name << " result: " << result << '\n';
        }
    }

    // Видалити всі активні компоненти
    void clear() {
        components.clear();
        pipes.clear();
        results.clear();
        currentGroup.clear();  // Очищаємо групу
    }
};

int main() {
    Manager mgr;
    std::string input;

    std::cout << "Command-line interface started. Type 'help' for commands.\n";

    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);

        // Розбиття вводу на слова
        std::istringstream iss(input);
        std::vector<std::string> tokens{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};

        if (tokens.empty()) continue;

        // Команди
        if (tokens[0] == "help") {
            std::cout << "Available commands:\n";
            std::cout << "  group <name>        - Create a new task group (only one group is allowed).\n";
            std::cout << "  new <component_name> <arg> - Add new component to the current group with argument.\n";
            std::cout << "  run                 - Run all added components.\n";
            std::cout << "  status              - Show status of active components.\n";
            std::cout << "  summary             - Show results of all components.\n";
            std::cout << "  clear               - Clear all active components and group.\n";
            std::cout << "  exit                - Exit the program.\n";
        } else if (tokens[0] == "group") {
            if (tokens.size() != 2) {
                std::cerr << "Usage: group <group_name>\n";
                continue;
            }
            mgr.createGroup(tokens[1]);
        } else if (tokens[0] == "new") {
            if (tokens.size() != 3) {
                std::cerr << "Usage: new <component_name> <arg>\n";
                continue;
            }
            std::string componentName = tokens[1];
            int arg = std::stoi(tokens[2]);

            // Додавання компонентів за типом
            if (componentName == "f1") {
                mgr.addComponent(componentName, function_f, arg);
                std::cout << "Component " << componentName << " added to group " << currentGroup << " with argument " << arg << ".\n";
            } else if (componentName == "f2") {
                mgr.addComponent(componentName, function_g, arg);
                std::cout << "Component " << componentName << " added to group " << currentGroup << " with argument " << arg << ".\n";
            } else if (componentName == "f3") {
                mgr.addComponent(componentName, function_h, arg);
                std::cout << "Component " << componentName << " added to group " << currentGroup << " with argument " << arg << ".\n";
            } else {
                std::cerr << "Unknown component name: " << componentName << ". Use f1, f2, or f3.\n";
            }
        } else if (tokens[0] == "run") {
            mgr.run();
        } else if (tokens[0] == "summary") {
            mgr.summary();
        } else if (tokens[0] == "status") {
            mgr.status();
        } else if (tokens[0] == "clear") {
            mgr.clear();
            std::cout << "Cleared all components and group.\n";
        } else if (tokens[0] == "exit") {
            std::cout << "Exiting...\n";
            break;
        } else {
            std::cerr << "Unknown command. Type 'help' for commands.\n";
        }
    }

    return 0;
}
