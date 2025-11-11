//
// Created by bobenade on 11/11/2025.
//


#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>


std::mutex gLock;   //globalny zamok
std::condition_variable gConditionVariable; //globalna podmienena premenna

int main() {

    int result = 0; //na vypocty hodnot alebo vratenie 42
    bool notified = false;  //cast ktora bude komunikovat medzi dvomi threadmi ci praca bola vykonana pracujucim threadom a potom mozeme zobudit jeden alebo viac threadov a notifikovat ich o tom ze praca bola vykonana a ina cast programu moze pokracovat

    // Reporting thread
    //must wait on work, done by the working thread
    std::thread reporter([&]] {


    });

    //Working thread
    std::thread worker([&]] {
        std::unique_lock<std::mutex> lock(gLock);
        //Do our work, because we have the lock
        result = 42+1+7;

        //our work is done
        notified = true;
    });
    //oba thready su synchronizovane a vieme ze ako prva bude pracovat worker thread a reporter musi pockat kym worker dorobi

}