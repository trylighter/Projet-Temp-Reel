#include "../header/functions.h"

char mode_start;
int compteur=0;
Camera m_camera;
Arene monArene;
extern int arene_validee=0;
extern int mode_camera=0; 

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
            rt_mutex_acquire(&mutex_send_message, TM_INFINITE);
            send_message_to_monitor(msg.header, msg.data);
            rt_mutex_release(&mutex_send_message);
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
        if (err < 0) {
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
                rt_sem_broadcast(&sem_startRobot);

            }
            else if(msg.data[0] == DMB_START_WITH_WD) {
                rt_sem_broadcast(&sem_startRobot);
                rt_sem_v(&sem_startwithWD);
            }
            else if ((msg.data[0] == DMB_GO_BACK)
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
        }
        else if(strcmp(msg.header, HEADER_MTS_CAMERA) == 0)
        {
            if (msg.data[0] == CAM_OPEN) {
                rt_sem_v(&sem_startCam);
                printf("Message d'ouverture recu\n");
            }
            else if (msg.data[0] == CAM_COMPUTE_POSITION) {
                printf("Message de calcul de position recu\n");
                rt_mutex_acquire(&mutex_camera, TM_INFINITE);
                mode_camera=2 ;
                rt_mutex_release(&mutex_camera);
            }
            else if (msg.data[0] == CAM_STOP_COMPUTE_POSITION) {
                rt_mutex_acquire(&mutex_camera, TM_INFINITE);
                mode_camera=1 ;
                rt_mutex_release(&mutex_camera);
                printf("Message d'arret du calcul de position\n");
            }
            else if (msg.data[0] == CAM_ASK_ARENA) {
                rt_mutex_acquire(&mutex_camera, TM_INFINITE);
                mode_camera=3 ;
                rt_mutex_release(&mutex_camera);
                printf("Message d'arret du calcul de position\n");
            }  
            else if (msg.data[0]==CAM_CLOSE)
            {
                close_camera(&m_camera);
                send_message_to_monitor(HEADER_STM_ACK);
                rt_mutex_acquire(&mutex_camera, TM_INFINITE);
                mode_camera=0 ;
                rt_mutex_release(&mutex_camera);
            }
            else if (msg.data[0]==CAM_ARENA_CONFIRM)
            {
                rt_mutex_acquire(&mutex_arene, TM_INFINITE);
                arene_validee=1 ;
                rt_mutex_release(&mutex_arene);
                printf("Arene validee\n");
                rt_sem_v(&sem_confirmArena);
            }
            else if (msg.data[0]==CAM_ARENA_INFIRM)
            {
                rt_mutex_acquire(&mutex_arene, TM_INFINITE);
                arene_validee=0 ;
                rt_mutex_release(&mutex_arene);
                printf("Arene non validee\n");
                rt_sem_v(&sem_confirmArena);
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
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /* PERIODIC START */
/*#ifdef _WITH_TRACE_
    printf("%s: start period\n", info.name);
#endif*/
    rt_task_set_periodic(NULL, TM_NOW, 100000000);
    while (1) {
/*#ifdef _WITH_TRACE_
        printf("%s: Wait period \n", info.name);
#endif*/
        rt_task_wait_period(NULL);
/*#ifdef _WITH_TRACE_
        printf("%s: Periodic activation\n", info.name);
        printf("%s: move equals %c\n", info.name, move);
#endif*/
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        if (robotStarted) {
            rt_mutex_acquire(&mutex_move, TM_INFINITE);
            send_command_to_robot(move);
            rt_mutex_release(&mutex_move);
/*#ifdef _WITH_TRACE_
            printf("%s: the movement %c was sent\n", info.name, move);
#endif            */
        }
        rt_mutex_release(&mutex_robotStarted);
    }
}

void write_in_queue(RT_QUEUE *queue, MessageToMon msg) {
    void *buff;
    buff = rt_queue_alloc(&q_messageToMon, sizeof (MessageToMon));
    memcpy(buff, &msg, sizeof (MessageToMon));
    rt_queue_send(&q_messageToMon, buff, sizeof (MessageToMon), Q_NORMAL);
}




void f_openCamera (void *arg) {
    /* INIT */
    rt_sem_p(&sem_barrier, TM_INFINITE);    
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    printf("Attente sem_startCam\n");
    while (1) {
        rt_sem_p(&sem_startCam, TM_INFINITE);
        printf("Sem_startCam recu\n");  
        if (open_camera(&m_camera)==-1){ //echec ouverture camera
            printf("Probleme ouverture camera \n");
            MessageToMon msg;
            set_msgToMon_header(&msg, HEADER_STM_NO_ACK);
            write_in_queue(&q_messageToMon, msg);
        }
        else { //ouverture camera OK
            printf("Camera ouverte\n");
            rt_mutex_acquire(&mutex_send_message, TM_INFINITE);
            send_message_to_monitor(HEADER_STM_ACK);
            rt_mutex_release(&mutex_send_message);
            rt_mutex_acquire(&mutex_camera, TM_INFINITE);
            mode_camera=1 ;
            rt_mutex_release(&mutex_camera);
        }
    } 
}


void f_camera (void *arg){
    /* INIT */
    RT_TASK_INFO info;
    rt_sem_p(&sem_barrier, TM_INFINITE);
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    /* PERIODIC START */
    rt_task_set_periodic(NULL, TM_NOW, 100000000);
    Image image_in;
    Image image_out;
    Jpg image_comp;
    int detection_position(0);
    int mode;
    int arene_ok;
    while (1) {
        rt_task_wait_period(NULL);
        rt_mutex_acquire(&mutex_camera, TM_INFINITE);
        mode = mode_camera ;
        rt_mutex_release(&mutex_camera);
        rt_mutex_acquire(&mutex_arene, TM_INFINITE);
        arene_ok = arene_validee ;
        rt_mutex_release(&mutex_arene);
        if (mode==1) { //en mode 1 la camera envoie les images (avec arene dessinee dessus si l'arene a ete validee)
            detection_position=1;
            get_image(&m_camera,&image_in,NULL);
            if (arene_ok==1) { //Si l'arene a ete validee aupravant, on dessine l'arene sur l'image
                draw_arena(&image_in, &image_out, &monArene);
            }
            else {
                image_out=image_in;
            }
            compress_image(&image_out, &image_comp);
            
            rt_mutex_acquire(&mutex_send_message, TM_INFINITE);
            if (send_message_to_monitor(HEADER_STM_IMAGE,&image_comp) != 0) {
                printf("Erreur send image\n");
            };
            rt_mutex_release(&mutex_send_message);
        }
        else if (mode==2) { //Calcul de la position et envoi de la position et image avec position dessinee
            detection_position=2; //La variable detection_position permet de retourner dans le mode dans lequel on etait avant la boucle de detection de l'arene
            Image imagePos;
            Jpg imagePos_comp;
            MessageToMon pos;
            Position maPosition ;
            get_image(&m_camera,&image_in,NULL);
            if (arene_ok==1) { //Si l'arene a ete validee aupravant, on dessine l'arene sur l'image.
                if (detect_position(&image_in, &maPosition, &monArene)!=1) {
                    draw_arena(&image_in, &image_out, &monArene);
                }
                else {
                    draw_arena(&image_in, &image_out, &monArene);
                    //draw_position(&imagePos, &image_out, &maPosition);
                }
            }
            else {
                if (detect_position(&image_in, &maPosition) != 1) {
                    image_out=image_in;
                }
                else {
                    draw_position(&image_in, &imagePos, &maPosition);
                    image_out=imagePos;
                }
            }
            compress_image(&image_out, &imagePos_comp);
            rt_mutex_acquire(&mutex_send_message, TM_INFINITE);
            send_message_to_monitor(HEADER_STM_IMAGE, &imagePos_comp);
            rt_mutex_release(&mutex_send_message);
            rt_mutex_acquire(&mutex_send_message, TM_INFINITE);
            send_message_to_monitor(HEADER_STM_POS,&maPosition);
            rt_mutex_release(&mutex_send_message);
        }
        else if (mode==3){ //Mode en cas de demande de detection de l'arene
            Image image_ar;
            Jpg image_comp;
            MessageFromMon msg;
            char *message;
            get_image(&m_camera,&image_in,NULL);
            if (detect_arena(&image_in, &monArene)==0) {
                draw_arena(&image_in, &image_ar, &monArene);
                compress_image(&image_ar, &image_comp);
                rt_mutex_acquire(&mutex_send_message, TM_INFINITE);
                send_message_to_monitor(HEADER_STM_IMAGE,&image_comp);
                rt_mutex_release(&mutex_send_message);
                rt_sem_p(&sem_confirmArena, TM_INFINITE);
            }
            else { //Aucune arene detectee sur l'image
                printf ("Aucune arene detectee\n");
                rt_mutex_acquire(&mutex_send_message, TM_INFINITE);
                send_message_to_monitor(HEADER_STM_MES, "Aucune arene detectee");
                rt_mutex_release(&mutex_send_message);
            }
            rt_mutex_acquire(&mutex_camera, TM_INFINITE);
            mode_camera = detection_position; //on retourne dans le mode d'envoi des images precedant la demande d'arene
            rt_mutex_release(&mutex_camera);
        }
    }
}


void f_batterie(void *arg){
    RT_TASK_INFO info;
    MessageToMon msg;
    int niveau_bat;
    int robot_status;
    char message[10];
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);
    rt_task_set_periodic(NULL, TM_NOW, 500000000);
    while(1)
    {
        // Timer Période
        rt_task_wait_period(NULL);
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE); 
        robot_status = robotStarted;
        rt_mutex_release(&mutex_robotStarted);
        if(robot_status == 1){
            //Envoi du message :
            niveau_bat = send_command_to_robot(DMB_GET_VBAT); //recupere aupres du robot le niveau de batterie
            if(niveau_bat < 0) //Si < 0 alors probleme de communication
            {
                rt_sem_v(&sem_pbComm);
            }
            else
            { //autrement, on remet le compteur de problemes a zero
                rt_mutex_acquire(&mutex_compteur, TM_INFINITE);
                compteur = 0;
                rt_mutex_release(&mutex_compteur);
            }
            sprintf(message,"%d",niveau_bat);
            set_msgToMon_header(&msg, HEADER_STM_BAT);
            set_msgToMon_data(&msg, (void *) message);
            if(send_message_to_monitor(msg.header, msg.data)<0)
            {
                rt_sem_v(&sem_errS); //probleme envoi au moniteur, notification a la tache de surveillance serveur-superviseur du pb d'envoi
            }
        }
    }
}


void f_watchComServer(void *arg)
{
    rt_sem_p(&sem_barrier, TM_INFINITE);
    rt_sem_p(&sem_errS, TM_INFINITE); //attente d'un probleme socket d'envoi
    rt_sem_p(&sem_errR, TM_INFINITE); //attente d'un probleme socket de reception
    printf("Connexion perdue");
    rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
    robotStarted = 0;
    rt_mutex_release(&mutex_robotStarted);
    rt_mutex_acquire(&mutex_camera, TM_INFINITE);
    mode_camera = 0;
    rt_mutex_release(&mutex_camera);
    close_communication_robot();
    close_camera(&m_camera);
    close_server();
    kill_nodejs();
    rt_mutex_acquire(&mutex_mode_reboot, TM_INFINITE);
    mode_reboot = 2; //reboot les threads aperiodiques, y compris le serveur
    rt_mutex_release(&mutex_mode_reboot);
    rt_sem_v(&sem_arretTache); //lance le reboot dans le main
}


void f_watchDog(void *arg)
{
    MessageToMon msg;
    int compteur_WD;
    RT_TASK_INFO info;
    rt_sem_p(&sem_barrier, TM_INFINITE);
    rt_task_inquire(NULL, &info);
    rt_task_set_periodic(NULL, TM_NOW, 100000000);
    rt_sem_p(&sem_startwithWD,TM_INFINITE); //attend une demande de demarrage du robot avec watchdog
    
    while(1)
    {
        rt_task_wait_period(NULL);
        compteur_WD=0;
        while(compteur_WD<4) //si le compteur depasse 3 : probleme dans la communication
        {
            rt_task_wait_period(NULL);
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
        printf("WATCHDOG : PROBLEME COMMUNICATION ROBOT\n");
        set_msgToMon_header(&msg, HEADER_STM_LOST_DMB);
        if (send_message_to_monitor(msg.header,NULL)<0) {
            rt_sem_v(&sem_errS);
        }
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
        compteur = 0;
        rt_mutex_release(&mutex_compteur);
        while(compteur<4)
        {
            rt_sem_p(&sem_pbComm,TM_INFINITE); //probleme de communication survenu dans une tache
            rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE); 
            if(robotStarted == 1)
            {
                rt_mutex_acquire(&mutex_compteur, TM_INFINITE);
                compteur++;
                rt_mutex_release(&mutex_compteur);
            }
            rt_mutex_release(&mutex_robotStarted);
        }
        printf("Probleme connexion robot superviseur\n");
        rt_mutex_acquire(&mutex_camera, TM_INFINITE);
        mode_camera = 0;
        rt_mutex_release(&mutex_camera);
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        robotStarted = 0;
        rt_mutex_release(&mutex_robotStarted);
        set_msgToMon_header(&msg, HEADER_STM_LOST_DMB);
        if(send_message_to_monitor(msg.header,NULL)<0) {
            rt_sem_v(&sem_errS);
        }
        close_communication_robot();
        close_camera(&m_camera);
        rt_mutex_acquire(&mutex_mode_reboot, TM_INFINITE);
        mode_reboot = 1; //reboot des threads aperiodiques, sans la tache serveur
        rt_mutex_release(&mutex_mode_reboot);
        rt_sem_v(&sem_arretTache);
    }
}

