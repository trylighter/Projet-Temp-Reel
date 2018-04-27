/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   functions.h
 * Author: pehladik
 *
 * Created on 15 janvier 2018, 12:50
 */

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/mman.h>
#include <alchemy/task.h>
#include <alchemy/timer.h>
#include <alchemy/mutex.h>
#include <alchemy/sem.h>
#include <alchemy/queue.h>

#include "../../src/monitor.h"    
#include "../../src/robot.h"
#include "../../src/image.h"
#include "../../src/message.h"

extern RT_TASK th_server;
extern RT_TASK th_sendToMon;
extern RT_TASK th_receiveFromMon;
extern RT_TASK th_openComRobot;
extern RT_TASK th_startRobot;
extern RT_TASK th_move;
extern RT_TASK th_openCamera;
extern RT_TASK th_camera;
extern RT_TASK th_batterie;
extern RT_TASK th_watchComServeur;
extern RT_TASK th_watchComRobot;
extern RT_TASK th_watchDog;

extern RT_MUTEX mutex_robotStarted;
extern RT_MUTEX mutex_move;
extern RT_MUTEX mutex_send_message;
extern RT_MUTEX mutex_camera;
extern RT_MUTEX mutex_arene;
extern RT_MUTEX mutex_compteur;
extern RT_MUTEX mutex_mode_reboot;

extern RT_SEM sem_barrier;
extern RT_SEM sem_openComRobot;
extern RT_SEM sem_serverOk;
extern RT_SEM sem_startRobot;
extern RT_SEM sem_cameraStarted;
extern RT_SEM sem_startCam;
extern RT_SEM sem_confirmArena;
extern RT_SEM sem_startwithWD;
extern RT_SEM sem_errS;
extern RT_SEM sem_errR;
extern RT_SEM sem_pbComm;
extern RT_SEM sem_arretTache;


extern RT_QUEUE q_messageToMon;

extern int etatCommMoniteur;
extern int robotStarted;
extern char move;
extern int mode_reboot;

extern int MSG_QUEUE_SIZE;

extern int PRIORITY_TSERVER;
extern int PRIORITY_TOPENCOMROBOT;
extern int PRIORITY_TMOVE;
extern int PRIORITY_TSENDTOMON;
extern int PRIORITY_TRECEIVEFROMMON;
extern int PRIORITY_TSTARTROBOT;
extern int PRIORITY_TCAMERA;
extern int PRIORITY_TOPENCAMERA;
extern int PRIORITY_TBATTERIE;
extern int PRIORITY_TWATCHCOMSERVER;
extern int PRIORITY_TWATCHCOMROBOT;
extern int PRIORITY_TWATCHDOG;

void f_server(void *arg);
void f_sendToMon(void *arg);
void f_receiveFromMon(void *arg);
void f_openComRobot(void * arg);
void f_move(void *arg);
void f_startRobot(void *arg);

/**
 * \fn void f_openCamera(void)
 * \brief Ouvre une camera et met a jour le pointeur m_camera
 * vers une camera ouverte. Affiche un message d'erreur en cas d'echec.
 */
void f_openCamera(void *arg);

/**
 * \fn void f_camera(void)
 * \brief Fonction periodique de recuperation et d'envoi des images,
 * de la position et de l'arene selon le mode.
 */
void f_camera(void *arg);

/**
 * \fn void f_batterie(void)
 * \brief Actualise l'etat de la batterie sur le moniteur toutes les 500ms
 */
void f_batterie(void *arg);

/**
 * \fn void f_watchComServer(void)
 * \brief Surveille la connexion entre le superviseur et le serveur
 */
void f_watchComServer(void *arg);

/**
 * \fn void f_watchComRobot(void)
 * \brief Surveille la connexion entre le superviseur et le robot
 */
void f_watchComRobot(void *arg);

/**
 * \fn void f_watchComRobot(void)
 * \brief Definit un watchdog permettant de tester periodiquement le bon
 * etat de la communication entre le superviseur et le robot
 */
void f_watchDog(void *arg);

#endif /* FUNCTIONS_H */

