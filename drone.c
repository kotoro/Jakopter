#include "drone.h"
#include "navdata.h"

#define HEAD_REF "REF"
#define HEAD_PCMD "PCMD"
#define SIZE_INT 10
#define ARGS_MAX 7

/*commandes pour décoller et atterrir*/
char ref_cmd[PACKET_SIZE];
char  *takeoff_arg="290718208",
	 *land_arg="290717696";

/*N° de commande actuel*/
int cmd_no_sq = 0;
/*commande en cours d'envoi, et ses arguments*/
char *cmd_current = NULL; 
char * cmd_current_args[ARGS_MAX];

/*Thread qui se charge d'envoyer régulièrement des commandes pour rester co avec le drone*/
pthread_t cmd_thread;
int stopped = 1;      //Guard that stops any function if connection isn't initialized.
static pthread_mutex_t mutex_cmd = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_stopped = PTHREAD_MUTEX_INITIALIZER;

/* Change la commande courante : prend le nom de la commande
et une chaîne avec les arguments
 * \param cmd_type header de la forme AT*SOMETHING
 * \param args code commande à transmettre (takeoff_arg)
*/
int set_cmd(char* cmd_type, char** args, int nb_args) {
	if (nb_args > ARGS_MAX) return -1;
	pthread_mutex_lock(&mutex_cmd);
	cmd_current = cmd_type;

	int i = 0;
	for (i=0; (i<ARGS_MAX) && (i<nb_args); i++) {
		strncpy(cmd_current_args[i],args[i],SIZE_INT);
	}

	if (i<ARGS_MAX) {
		cmd_current_args[i] = NULL;
	}

	pthread_mutex_unlock(&mutex_cmd);
	return 0;
}

// TODO: Pensez à vider cmd 
void gen_cmd(char * cmd, char* cmd_type, int no_sq, char** args){
	char buf[SIZE_INT];
	snprintf(buf,SIZE_INT,"%d",no_sq);
	cmd = strncat(cmd, "AT*", PACKET_SIZE);
	cmd = strncat(cmd, cmd_type, PACKET_SIZE);
	cmd = strncat(cmd, "=", PACKET_SIZE);
	cmd = strncat(cmd, buf, PACKET_SIZE);

	int i = 0;
	while((args[i] != NULL) && (i < ARGS_MAX)) {
		cmd = strncat(cmd, ",", PACKET_SIZE);
		cmd = strncat(cmd, args[i], PACKET_SIZE);
		i++;
	}
}

/*Envoie la commande courante, et incrémente le compteur*/
int send_cmd() {
	pthread_mutex_lock(&mutex_cmd);

	if(cmd_current != NULL) {
		memset(ref_cmd, 0, PACKET_SIZE);
		ref_cmd[0] = '\0';
		gen_cmd(ref_cmd,cmd_current,cmd_no_sq,cmd_current_args);
		cmd_no_sq++;

		int ret = sendto(sock_cmd, ref_cmd, PACKET_SIZE, 0, (struct sockaddr*)&addr_drone, sizeof(addr_drone));
		pthread_mutex_unlock(&mutex_cmd);

		return ret;
	}

	pthread_mutex_unlock(&mutex_cmd);

	return 0;
}

/*Fonction de cmd_thread*/
void* cmd_routine(void* args) {
	struct timespec itv = {0, TIMEOUT_CMD};
	pthread_mutex_lock(&mutex_stopped);
	while(!stopped) {
		pthread_mutex_unlock(&mutex_stopped);

		if(send_cmd() < 0)
			perror("Erreur d'envoi au drone");
		nanosleep(&itv, NULL);

		pthread_mutex_lock(&mutex_stopped);
	}
	pthread_mutex_unlock(&mutex_stopped);

	pthread_exit(NULL);
}

/*créer un socket + initialiser l'adresse du drone et du client ; remettre le nb de commandes à 1.
Démarrer le thread de commande.
Appelle stop si le thread est déjà en train de tourner.*/
int jakopter_connect() {

	//stopper la com si elle est déjà initialisée
	pthread_mutex_lock(&mutex_stopped);
	if(!stopped) {
		pthread_mutex_unlock(&mutex_stopped);
		jakopter_disconnect();

	}
	else
		pthread_mutex_unlock(&mutex_stopped);

	addr_drone.sin_family      = AF_INET;
	addr_drone.sin_addr.s_addr = inet_addr(WIFI_ARDRONE_IP);
	addr_drone.sin_port        = htons(PORT_CMD);

	addr_client.sin_family      = AF_INET;
	addr_client.sin_addr.s_addr = htonl(INADDR_ANY);
	addr_client.sin_port        = htons(PORT_CMD);

	sock_cmd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sock_cmd < 0) {
		fprintf(stderr, "Erreur, impossible d'établir le socket\n");
		return -1;
	}

	//bind du socket client pour le forcer sur le port choisi
	if(bind(sock_cmd, (struct sockaddr*)&addr_client, sizeof(addr_client)) < 0) {
		fprintf(stderr, "Erreur : impossible de binder le socket au port %d\n", PORT_CMD);
		return -1;
	}

	//réinitialiser les commandes
	pthread_mutex_lock(&mutex_cmd);
	cmd_no_sq = 1;
	cmd_current = NULL;

	pthread_mutex_unlock(&mutex_cmd);

	pthread_mutex_lock(&mutex_stopped);
	stopped = 0;
	pthread_mutex_unlock(&mutex_stopped);

	int navdata_status = navdata_connect();
	if(navdata_status == -1) {
		perror("Erreur de connexion navdata");
		return -1;
	}


	//démarrer le thread
	if(pthread_create(&cmd_thread, NULL, cmd_routine, NULL) < 0) {
		perror("Erreur création thread");
		return -1;
	}

	return 0;
}

/*faire décoller le drone (échoue si pas init).*/
int jakopter_takeoff() {

	//vérifier qu'on a initialisé
	pthread_mutex_lock(&mutex_stopped);
	if(!cmd_no_sq || stopped) {
		pthread_mutex_unlock(&mutex_stopped);
		fprintf(stderr, "Erreur : la communication avec le drone n'a pas été initialisée\n");
		return -1;
	}
	else
		pthread_mutex_unlock(&mutex_stopped);

	//changer la commande
	//takeoff = 0x11540200, land = 0x11540000

	set_cmd(HEAD_REF, &takeoff_arg,1);
	return 0;
}

/*faire atterrir le drone*/
int jakopter_land() {
	//vérifier qu'on a initialisé
	pthread_mutex_lock(&mutex_stopped);

	if(!cmd_no_sq || stopped) {
		pthread_mutex_unlock(&mutex_stopped);

		fprintf(stderr, "Erreur : la communication avec le drone n'a pas été initialisée\n");
		return -1;
	}
	else
		pthread_mutex_unlock(&mutex_stopped);

	set_cmd(HEAD_REF, &land_arg, 1);
	return 0;
}

int jakopter_rotate_left() {
	//vérifier qu'on a initialisé
	pthread_mutex_lock(&mutex_stopped);
	if(!cmd_no_sq || stopped) {
		pthread_mutex_unlock(&mutex_stopped);

		fprintf(stderr, "Erreur : la communication avec le drone n'a pas été initialisée\n");
		return -1;
	}
	else
		pthread_mutex_unlock(&mutex_stopped);

	char * args[] = {"1","0","0","0","-1085485875"};
	set_cmd(HEAD_REF, args,5);
	return 0;
}

int jakopter_rotate_right() {
	//vérifier qu'on a initialisé
	pthread_mutex_lock(&mutex_stopped);
	if(!cmd_no_sq || stopped) {
		pthread_mutex_unlock(&mutex_stopped);

		fprintf(stderr, "Erreur : la communication avec le drone n'a pas été initialisée\n");
		return -1;
	}
	else
		pthread_mutex_unlock(&mutex_stopped);

	char * args[] = {"1","0","0","0","106199773"};
	set_cmd(HEAD_REF, args,5);
	return 0;
}

int jakopter_forward() {
	//vérifier qu'on a initialisé
	pthread_mutex_lock(&mutex_stopped);
	if(!cmd_no_sq || stopped) {
		pthread_mutex_unlock(&mutex_stopped);

		fprintf(stderr, "Erreur : la communication avec le drone n'a pas été initialisée\n");
		return -1;
	}
	else
		pthread_mutex_unlock(&mutex_stopped);

	char * args[] = {"1","0","-1102263091","0","0"};
	set_cmd(HEAD_REF, args,1);
	return 0;
}

int jakopter_backward() {
	//vérifier qu'on a initialisé
	pthread_mutex_lock(&mutex_stopped);
	if(!cmd_no_sq || stopped) {
		pthread_mutex_unlock(&mutex_stopped);

		fprintf(stderr, "Erreur : la communication avec le drone n'a pas été initialisée\n");
		return -1;
	}
	else
		pthread_mutex_unlock(&mutex_stopped);

	char * args[] = {"1","0","0","104522055","0","0"};
	set_cmd(HEAD_REF, args,1);
	return 0;
}

/*Arrêter le thread principal (fin de la co au drone)*/
int jakopter_disconnect() {
	pthread_mutex_lock(&mutex_stopped);
	if(!stopped) {
		stopped = 1;
		pthread_mutex_unlock(&mutex_stopped);

		close(sock_cmd);
		navdata_disconnect();
		return pthread_join(cmd_thread, NULL);
	}
	else {
		pthread_mutex_unlock(&mutex_stopped);

		fprintf(stderr, "Erreur : la communication est déjà stoppée\n");
		return -1;
	}
}


/*Obtenir le nombre actuel de commandes*/
int jakopter_get_no_sq() {
	return cmd_no_sq;
}
