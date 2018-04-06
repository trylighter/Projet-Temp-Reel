#include "../header/functions.h"

char mode_start;
int compteur=0;

void write_in_queue(RT_QUEUE *, MessageToMon);

void f_server(void *arg) {
    int err;
    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);

    err = run_nodejs("/usr/local/bin/node", "/home/pi/Interface_Robot/server.js");

    if (err < 0) {
        printf("Failed to start nodejs: %s\n", strerror(-err));
        exit(EXIT_FAILURE);
    } else {
#ifdef _WITH_TRACE_
        printf("%s: nodejs started\n", info.name);
#endif
        open_server();
        rt_sem_broadcast(&sem_serverOk);
    }
}

void f_sendToMon(void * arg) {
    int err;
    MessageToMon msg;

    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);

#ifdef _WITH_TRACE_
    printf("%s : waiting for sem_serverOk\n", info.name);
#endif
    rt_sem_p(&sem_serverOk, TM_INFINITE);
    while (1) {

#ifdef _WITH_TRACE_
        printf("%s : waiting for a message in queue\n", info.name);
#endif
        if (rt_queue_read(&q_messageToMon, &msg, sizeof (MessageToRobot), TM_INFINITE) >= 0) {
#ifdef _WITH_TRACE_
            printf("%s : message {%s,%s} in queue\n", info.name, msg.header, msg.data);
#endif

    
            if(send_message_to_monitor(msg.header, msg.data)<0)
            {
                rt_sem_v(&sem_errS);
            }
            free_msgToMon_data(&msg);
            rt_queue_free(&q_messageToMon, &msg);
        } else {
            printf("Error msg queue write: %s\n", strerror(-err));
        }
    }
}

void f_receiveFromMon(void *arg) {
    MessageFromMon msg;
    int err;

    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);

#ifdef _WITH_TRACE_
    printf("%s : waiting for sem_serverOk\n", info.name);
#endif
    rt_sem_p(&sem_serverOk, TM_INFINITE);
    do {
#ifdef _WITH_TRACE_
        printf("%s : waiting for a message from monitor\n", info.name);
#endif
        err = receive_message_from_monitor(msg.header, msg.data);
        if(err<0)
        {
            rt_sem_v(&sem_errR);
        }
#ifdef _WITH_TRACE_
        printf("%s: msg {header:%s,data=%s} received from UI\n", info.name, msg.header, msg.data);
#endif
        if (strcmp(msg.header, HEADER_MTS_COM_DMB) == 0) {
            if (msg.data[0] == OPEN_COM_DMB) { // Open communication supervisor-robot
#ifdef _WITH_TRACE_
                printf("%s: message open Xbee communication\n", info.name);
#endif
                //rt_sem_v(&sem_openComRobot);
                rt_sem_broadcast(&sem_openComRobot);
            }
        } else if (strcmp(msg.header, HEADER_MTS_DMB_ORDER) == 0) {
            if (msg.data[0] == DMB_START_WITHOUT_WD) { // Start robot
#ifdef _WITH_TRACE_
                printf("%s: message start robot\n", info.name);
#endif 
                rt_sem_v(&sem_startRobot);

            } else if ((msg.data[0] == DMB_GO_BACK)
                    || (msg.data[0] == DMB_GO_FORWARD)
                    || (msg.data[0] == DMB_GO_LEFT)
                    || (msg.data[0] == DMB_GO_RIGHT)
                    || (msg.data[0] == DMB_STOP_MOVE)) {

                rt_mutex_acquire(&mutex_move, TM_INFINITE);
                move = msg.data[0];
                rt_mutex_release(&mutex_move);
#ifdef _WITH_TRACE_
                printf("%s: message update movement with %c\n", info.name, move);
#endif

            }
            else if(msg.data[0] == DMB_START_WITH_WD) {
                rt_sem_v(&sem_startwithWD);
                rt_sem_v(&sem_startRobot);
            }
        }
    } while (err > 0);

}

void f_openComRobot(void * arg) {
    int err;

    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);

    while (1) {
#ifdef _WITH_TRACE_
        printf("%s : Wait sem_openComRobot\n", info.name);
#endif
        rt_sem_p(&sem_openComRobot, TM_INFINITE);
#ifdef _WITH_TRACE_
        printf("%s : sem_openComRobot arrived => open communication robot\n", info.name);
#endif
        err = open_communication_robot();
        if (err == 0) {
#ifdef _WITH_TRACE_
            printf("%s : the communication is opened\n", info.name);
#endif
            MessageToMon msg;
            set_msgToMon_header(&msg, HEADER_STM_ACK);
            write_in_queue(&q_messageToMon, msg);
        } else {
            MessageToMon msg;
            set_msgToMon_header(&msg, HEADER_STM_NO_ACK);
            write_in_queue(&q_messageToMon, msg);
        }
    }
}

void f_startRobot(void * arg) {
    int err;

    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);

    while (1) {
#ifdef _WITH_TRACE_
        printf("%s : Wait sem_startRobot\n", info.name);
#endif
        rt_sem_p(&sem_startRobot, TM_INFINITE);
#ifdef _WITH_TRACE_
        printf("%s : sem_startRobot arrived => Start robot\n", info.name);
#endif
        err = send_command_to_robot(DMB_START_WITHOUT_WD);
        if (err == 0) {
#ifdef _WITH_TRACE_
            printf("%s : the robot is started\n", info.name);
#endif
            rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
            robotStarted = 1;
            rt_mutex_release(&mutex_robotStarted);
            MessageToMon msg;
            set_msgToMon_header(&msg, HEADER_STM_ACK);
            write_in_queue(&q_messageToMon, msg);
        } else {
            MessageToMon msg;
            set_msgToMon_header(&msg, HEADER_STM_NO_ACK);
            write_in_queue(&q_messageToMon, msg);
        }
    }
}

void f_move(void *arg) {
 
    /* INIT */
  /*  RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);*/

    // /* PERIODIC START */
/*#ifdef _WITH_TRACE_
    printf("%s: start period\n", info.name);
#endif
    rt_task_set_periodic(NULL, TM_NOW, 100000000);
    while (1) {
#ifdef _WITH_TRACE_
        printf("%s: Wait period \n", info.name);
#endif
        rt_task_wait_period(NULL);
#ifdef _WITH_TRACE_
        printf("%s: Periodic activation\n", info.name);
        printf("%s: move equals %c\n", info.name, move);
#endif
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        if (robotStarted) {
            rt_mutex_acquire(&mutex_move, TM_INFINITE);
            send_command_to_robot(move);
            rt_mutex_release(&mutex_move);
#ifdef _WITH_TRACE_
            printf("%s: the movement %c was sent\n", info.name, move);
#endif            
        }
        rt_mutex_release(&mutex_robotStarted);
    }*/
}

void f_batterie(void *arg){
    printf("\n\nTest \n\n\n");
    RT_TASK_INFO info;
    printf("\n\nTest2 \n\n\n");
    MessageToMon msg;
    int niveau_bat = 2;
    char message[10];
    //message = malloc(sizeof(char)*10);
    printf("\n\nTest3 \n\n\n");
    rt_task_inquire(NULL, &info);
    printf("\n\nInit %s\n\n\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);
    printf("\nEntree dans le while\n\n\n");
    rt_task_set_periodic(NULL, TM_NOW, 500000000);
    
    while(1)
    {
        // Timer Période
        rt_task_wait_period(NULL);
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE); 
        if(robotStarted == 1){
            //Envoi du message :

            niveau_bat = send_command_to_robot(DMB_GET_VBAT); 
            if(niveau_bat < 0)
            {
                rt_sem_v(&sem_pbComm);
            }
            else
            {
                rt_mutex_acquire(&mutex_compteur, TM_INFINITE);
                compteur = 0;
                rt_mutex_release(&mutex_compteur);
            }
            sprintf(message,"%d",niveau_bat);
            //printf("Niveau batterie : %d\n",niveau_bat);
            //fflush(stdout);    


            set_msgToMon_header(&msg, HEADER_STM_BAT);
            set_msgToMon_data(&msg, (void *) message);
            //write_in_queue(&q_messageToMon, msg);
            if(send_message_to_monitor(msg.header, msg.data)<0)
            {
                rt_sem_v(&sem_errS);
            }
        }
        else {
            //printf("Robot not started\n");
        }
        rt_mutex_release(&mutex_robotStarted);
    }
    
}

void f_watchComServer(void *arg)
{
    rt_sem_p(&sem_barrier, TM_INFINITE);
    rt_sem_p(&sem_errS, TM_INFINITE);
    rt_sem_p(&sem_errR, TM_INFINITE);
    printf("Connexion perdue");
    rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
    robotStarted = 0;
    rt_mutex_release(&mutex_robotStarted);
    rt_mutex_acquire(&mutex_camera, TM_INFINITE);
    mode_camera = 0;
    rt_mutex_release(&mutex_camera);
    printf("Connexion perdue \n");
    close_server();
    kill_nodejs();   
}


void f_watchDog(void *arg)
{
    MessageToMon msg;
    int compteur_WD;
    RT_TASK_INFO info;
    rt_sem_p(&sem_barrier, TM_INFINITE);
    rt_task_inquire(NULL, &info);
    printf("Prddzdzdz \n");
    rt_task_set_periodic(NULL, TM_NOW, 100000000);
    while(1)
    {
        rt_sem_p(&sem_startwithWD,TM_INFINITE);
        printf("Prob \n");
        rt_task_wait_period(NULL);
        compteur_WD=0;
            while(compteur_WD<4)
            {
                rt_task_wait_period(NULL);
                //printf("ProbProb \n");
                rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE); 
                if(robotStarted == 1){
                    if(send_command_to_robot(DMB_RELOAD_WD)!=ROBOT_OK)
                    {
                        compteur_WD++;
                    }
                }
                else
                {
                    compteur_WD = 0;
                }
                rt_mutex_release(&mutex_robotStarted);
            }
        
    // set_msgToMon_header(&msg, HEADER_STM_LOST_DMB);
   /*  if(send_message_to_monitor(msg.header,NULL)<0)
     {
         rt_sem_v(&sem_errS);
     }*/
        printf("ProbWatchdog \n");
     //close_communication_robot();
    }
}
void f_watchComRobot(void *arg)
{
    MessageToMon msg;
    RT_TASK_INFO info;
    rt_sem_p(&sem_barrier, TM_INFINITE);
    rt_task_inquire(NULL, &info);
    while(1)
    {
        rt_mutex_acquire(&mutex_compteur, TM_INFINITE);
        compteur =0;
        rt_mutex_release(&mutex_compteur);
        while(compteur<4)
        {
            rt_sem_p(&sem_pbComm,TM_INFINITE);
            printf("ProbProb \n");
            rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE); 
            if(robotStarted == 1)
            {
                rt_mutex_acquire(&mutex_compteur, TM_INFINITE);
                compteur++;
                rt_mutex_release(&mutex_compteur);
            }
            rt_mutex_release(&mutex_robotStarted);
        }
        printf("ProbProbClaMerde \n");
    }
}


void write_in_queue(RT_QUEUE *queue, MessageToMon msg) {
    void *buff;
    buff = rt_queue_alloc(&q_messageToMon, sizeof (MessageToMon));
    memcpy(buff, &msg, sizeof (MessageToMon));
    rt_queue_send(&q_messageToMon, buff, sizeof (MessageToMon), Q_NORMAL);
}
