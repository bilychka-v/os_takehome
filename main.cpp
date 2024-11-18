#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <iterator>

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
    // Оновлений метод для додавання компонента
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
            activeProcesses[pid] = name; // Додаємо до активних процесів

            // Додаємо повідомлення про додавання компонента
            std::cout << "new component " << name << " " << arg << " added to group\n";
        }
    }


    // Запустити всі компоненти
    void run() {
        if (currentGroup.empty()) {
            std::cerr << "No group created yet. Please create a group first.\n";
            return;
        }

        std::cout << "Running group " << currentGroup << "...\n";

        for (const auto& [name, pid] : components) {
            std::cout << "Component " << name << " is running (PID: " << pid << ").\n";
        }
    }

    // Вивести статуси компонентів
    void status() {
        if (currentGroup.empty()) {
            std::cerr << "No group created yet. Please create a group first.\n";
            return;
        }

        for (auto it = activeProcesses.begin(); it != activeProcesses.end(); ) {
            pid_t pid = it->first;
            const std::string& name = it->second;
            int status;
            pid_t result = waitpid(pid, &status, WNOHANG); // Перевіряємо статус без блокування

            if (result == 0) {
                // Процес ще виконується
                std::cout << "Component " << name << " is still running (PID: " << pid << ").\n";
                ++it; // Перехід до наступного
            } else if (result == pid) {
                // Процес завершився
                if (WIFEXITED(status)) {
                    double resultValue;
                    if (read(pipes[name], &resultValue, sizeof(resultValue)) > 0) {
                        results[name] = resultValue; // Зберігаємо результат
                        std::cout << "Component " << name << " finished successfully, result: " << resultValue << "\n";
                    } else {
                        std::cerr << "Component " << name << " finished, but result is unavailable.\n";
                    }
                } else {
                    std::cerr << "Component " << name << " terminated abnormally.\n";
                }

                close(pipes[name]); // Закриваємо pipe
                pipes.erase(name);  // Видаляємо з мапи
                it = activeProcesses.erase(it); // Видаляємо зі списку активних процесів
            } else {
                // Процес не знайдено
                std::cerr << "Component " << name << " status unknown.\n";
                ++it;
            }
        }

        if (activeProcesses.empty()) {
            std::cout << "No active components are running.\n";
        }
    }

    // Перегляд результатів
    void summary() const {
        if (currentGroup.empty()) {
            std::cerr << "No group created yet. Please create a group first.\n";
            return;
        }

        for (const auto& [name, result] : results) {
            std::cout << "Component " << name << " result: " << result << "\n";
        }
    }

    // Видалити всі активні компоненти
    void clear() {
        components.clear();
        pipes.clear();
        results.clear();
        activeProcesses.clear();
        currentGroup.clear();  // Очищаємо групу
        std::cout << "Cleared all components and group.\n";
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
            int arg = std::stoi(tokens[2]);
            if (tokens[1] == "g") {
                mgr.addComponent(tokens[1], function_g, arg);
            } else if (tokens[1] == "h") {
                mgr.addComponent(tokens[1], function_h, arg);
            } else if (tokens[1] == "f") {
                mgr.addComponent(tokens[1], function_f, arg);
            } else {
                std::cerr << "Unknown component type: " << tokens[1] << "\n";
            }
        } else if (tokens[0] == "run") {
            mgr.run();
        } else if (tokens[0] == "status") {
            mgr.status();
        } else if (tokens[0] == "summary") {
            mgr.summary();
        } else if (tokens[0] == "clear") {
            mgr.clear();
        } else if (tokens[0] == "exit") {
            std::cout << "Exiting.\n";
            break;
        } else {
            std::cerr << "Unknown command: " << tokens[0] << ". Type 'help' for a list of commands.\n";
        }
    }

    // Завершуємо всі активні процеси перед виходом
    for (const auto& [pid, name] : activeProcesses) {
        kill(pid, SIGKILL);
        std::cerr << "Terminated active component: " << name << " (PID: " << pid << ").\n";
    }

    std::cout << "Program terminated. Goodbye!\n";
    return 0;
}


