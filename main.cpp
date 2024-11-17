#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <map>
#include <vector>
#include <string>
#include <sstream>

// Типи для функцій
using FunctionType = double (*)(int);

// Простий приклад функції обчислення
double function_g(int x) {
    return x * x * 1.5; // Наприклад, обчислення квадрата з коефіцієнтом
}

// Глобальні змінні для обробки сигналів
std::map<pid_t, std::string> activeProcesses;
bool cancelFlag = false;

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

// Клас для управління компонентами
class Manager {
private:
    std::map<std::string, pid_t> components; // Компоненти: ім'я -> PID
    std::map<std::string, int> pipes;        // Канали: ім'я -> pipe fd

public:
    Manager() {
        signal(SIGINT, signalHandler); // Реєстрація обробника сигналів
    }

    // Додати новий компонент
    void addComponent(const std::string& name, FunctionType func, int arg) {
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
            activeProcesses[pid] = name; // Додаємо у список активних
        }
    }

    // Запустити всі компоненти та отримати результати
    void run() {
        for (const auto& [name, pid] : components) {
            int status;
            waitpid(pid, &status, 0); // Чекаємо завершення процесу

            if (WIFEXITED(status)) {
                double result;
                read(pipes[name], &result, sizeof(result)); // Отримуємо результат
                std::cout << "Component " << name << " finished, result: " << result << '\n';
            } else {
                std::cerr << "Component " << name << " failed.\n";
            }

            close(pipes[name]); // Закриваємо pipe
            activeProcesses.erase(pid); // Видаляємо з активних
        }
        components.clear();
        pipes.clear();
    }

    // Вивести статуси компонентів
    void status() const {
        for (const auto& [name, pid] : components) {
            std::cout << "Component " << name << " running (PID: " << pid << ")\n";
        }
    }

    // Видалити всі активні компоненти
    void clear() {
        components.clear();
        pipes.clear();
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
            std::cout << "  new <name> <arg>  - Add new component with name and integer argument.\n";
            std::cout << "  run               - Run all added components.\n";
            std::cout << "  status            - Show status of active components.\n";
            std::cout << "  clear             - Clear all active components.\n";
            std::cout << "  exit              - Exit the program.\n";
        } else if (tokens[0] == "new") {
            if (tokens.size() != 3) {
                std::cerr << "Usage: new <name> <arg>\n";
                continue;
            }

            std::string name = tokens[1];
            int arg = std::stoi(tokens[2]);
            mgr.addComponent(name, function_g, arg);
            std::cout << "Added component: " << name << " with arg: " << arg << '\n';
        } else if (tokens[0] == "run") {
            mgr.run();
        } else if (tokens[0] == "status") {
            mgr.status();
        } else if (tokens[0] == "clear") {
            mgr.clear();
            std::cout << "Cleared all components.\n";
        } else if (tokens[0] == "exit") {
            std::cout << "Exiting...\n";
            break;
        } else {
            std::cerr << "Unknown command. Type 'help' for commands.\n";
        }
    }

    return 0;
}
