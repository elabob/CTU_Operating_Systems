//
// Created by bobenade on 19/10/2025.
//

#include <iostream>     //basics
#include <unistd.h>     // pre fork(), getpid(), getppid(), pipe(), dup2(), _exit()
#include <sys/types.h>  // pre typ pid_t
#include <signal.h>     // pre signal handling
#include <sys/wait.h>   // pre waitpid()
#include <cstdlib>      // pre rand(), srand()
#include <cstring>      // pre strlen()

// toto je handler pre signal SIGTERM
void gen_handler(int sig) {
    const char* msg = "GEN TERMINATED\n";  // sprava, ktoru vypisem ked proces dostane SIGTERM
    write(STDERR_FILENO, msg, strlen(msg)); // vypisem spravu na stderr
    _exit(0); // ukoncim sa okamzite, bez spustenia destruktorov
}


int main() {
    int fds[2]; // pole pre file descriptors roury (pipe)
    if(pipe(fds) == -1) _exit(2); // ak sa nepodari vytvorit rouru, okamzite koncim

    // vytvorim prvy child proces
    pid_t pid1 = fork();
    if(pid1 < 0) _exit(2); // chyba pri forku, koncim
    if(pid1 == 0) {
        // toto je kod prveho child procesu
        signal(SIGTERM, gen_handler); // nastavi handler pre SIGTERM
        close(fds[0]); // zavriem koniec na citanie, lebo budem len zapisovat
        if(dup2(fds[1], 1) == -1) _exit(2); // presmerujem stdout do pipe
        close(fds[1]); // povodny fd uz nepotrebujem
        while(1) {
            int a = rand()%4096, b = rand()%4096; // generujem dve nahodne cisla
            std::cout << a << " " << b << std::endl; // vypisem ich do pipe
            sleep(1); // pockam 1 sekundu
        }
    }

    // vytvorim druhy child proces
    pid_t pid2 = fork();
    if(pid2 < 0) _exit(2); // chyba pri forku, koncim
    if(pid2 == 0) {
        // toto je kod druheho child procesu
        close(fds[1]); // zavriem koniec na zapis, lebo budem len citat
        if(dup2(fds[0], 0) == -1) _exit(2); // presmerujem stdin z pipe
        close(fds[0]); // povodny fd uz nepotrebujem
        execl("./nsd","nsd",NULL); // nahradim tento proces programom nsd
        _exit(2); // ak execl skoncila chybou, ukoncim sa
    }

    // hlavny proces hned zavrie rouru, lebo ju nepotrebuje
    close(fds[0]);
    close(fds[1]);

    sleep(5); // pockam 5 sekund
    kill(pid1, SIGTERM); // poslem SIGTERM prvemu child procesu, aby sa ukoncil

    int status1, status2;
    waitpid(pid1,&status1,0); // pockam na prveho child
    waitpid(pid2,&status2,0); // pockam na druheho child

    // skontrolujem stav ukoncenia oboch child procesov
    if(WIFEXITED(status1) && WEXITSTATUS(status1)==0 &&
       WIFEXITED(status2) && WEXITSTATUS(status2)==0) {
        std::cout << "OK" << std::endl; // vsetko dobre
        return 0;
       } else {
           std::cout << "ERROR" << std::endl; // nieco sa pokazilo
           return 1;
       }
}