#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_BUF_LEN 1024

int ret, retl, sd, len, portaServer;
struct sockaddr_in srv_addr;
char *ipServer;
uint16_t lenm;

const int nInstruction = 8;
char buffer[MAX_BUF_LEN];
char comando[MAX_BUF_LEN];
char parametro1[MAX_BUF_LEN];
char parametro2[MAX_BUF_LEN];
char checkIP[MAX_BUF_LEN];
char response[MAX_BUF_LEN];
char token[10];

// Testo da stampare a video per "indirizzare" l'utente;
const char *instruction = {"\n"
						   "********************** GIOCO DEL LOTTO **********************\n"
						   "Sono disponibili i seguenti comandi:\n\n"

						   "1) !help <comando> --> mostra i dettagli di un comando\n"
						   "2) !signup <username> <password> --> crea un nuovo utente\n"
						   "3) !login <username> <password> --> autentica un utente\n"
						   "4) !invia_giocata g --> invia una giocata g al server\n"
						   "5) !vedi_giocate tipo --> visualizza le giocate precedenti dove tipo = {0,1}\n"
						   "\t\t\t  e permette di visualizzare le giocate passate ‘0’\n"
						   "\t\t\t  oppure le giocate attive ‘1’ (ancora non estratte)\n"
						   "6) !vedi_estrazione <n> <ruota> --> mostra i numeri delle ultime n estrazioni\n"
						   "\t\t\t\t    sulla ruota specificata\n"
						   "7) !esci --> termina il client\n\n"};

// Vettore di utilità per il check della correttezza del comando inserito;
char checkVector[][20] = {{"!help"}, {"!signup"}, {"!login"}, {"!invia_giocata"}, {"!vedi_giocate"}, {"!vedi_estrazione"}, {"!esci"}, {"!vedi_vincite"}};

// vettore di utilità per gestire la risposta del server;
char answer[][MAX_BUF_LEN] = {{"ATTENZIONE: Username già registrato, riprova!"},
							  {"INFO: Registrazione correttamente effettuata, procedi con il login e buon divertimento!"},
							  {"ATTENZIONE: Questo era il tuo ultimo tentativo di accesso, riprova tra 30 minuti"},
							  {"!invia_giocata"},
							  {"!vedi_giocate"},
							  {"!vedi_estrazione"},
							  {"!esci"}};

int checkInstruction(char *instr)
{
	int i = 0;
	int check = 0;

	for (i = 0; i < nInstruction; i++)
	{
		if ((strcmp(instr, (char *)checkVector[i])) == 0)
			check++;
	}

	{

		if (!check)
		{
			printf("ATTENZIONE: %s è un comando non valido!\n", instr);
			return 0;
		}
		/*********** PER FACILITARE LO SVILUPPO, RIMUOVERE *************/
		else
		{
			return 1;
		}
	}
}

void printInstruction()
{

	printf("%s", instruction);
	return;
}

void cleanBuf()
{

	strcpy(buffer, "");
	strcpy(comando, "");
	strcpy(parametro1, "");
	strcpy(parametro2, "");
	return;
}

int check(int var, char *tipo)
{

	if (var < 0)
	{
		printf("ATTENZIONE: %s\n", tipo);
		return 0;
	}

	return 1;
}

int checkResponse(char *comando, char *parametro1, char *parametro2)
{

	int risposta = 0;

	ret = recv(sd, (void *)&risposta, sizeof(uint16_t), 0);
	if (!check(ret, "Errore di comunicazione!"))
		return 0;

	if (risposta != 0)
	{

		if (strcmp(comando, "!signup") == 0)
		{
			printf("Utente registrato correttamente\n");
		}
		else if (strcmp(comando, "!login") == 0)
		{
			strcpy(token, "");
			strcpy(buffer, "");
			printf("LOGIN effettuato, Benvenuto %s\n", parametro1);
			ret = recv(sd, (void *)&lenm, sizeof(uint16_t), 0);
			if (!check(ret, "Errore di comunicazione!\n"))
				return 0;

			len = ntohs(lenm);

			ret = recv(sd, (void *)buffer, len, 0);
			if (!check(ret, "Errore di comunicazione!\n"))
				return 0;

			strcpy(response, "");

			sscanf((char *)buffer, "%s", token);
		}
 		else if (strcmp(comando, "!vedi_giocate") == 0 || strcmp(comando, "!vedi_estrazione") == 0 || strcmp(comando, "!vedi_vincite") == 0)
		{	

			ret = recv(sd, (void *)&lenm, sizeof(uint16_t), 0);
			if (!check(ret, "Errore di comunicazione!\n"))
				return 0;

			len = ntohs(lenm);

			ret = recv(sd, (void *)buffer, len, 0);
			if (!check(ret, "Errore di comunicazione!\n"))
				return 0;

			strcpy(response, "");

			printf("%s\n", (char*) buffer);
		}  
		return 1;
	}
	else
	{

		ret = recv(sd, (void *)&lenm, sizeof(uint16_t), 0);
		if (!check(ret, "Errore di comunicazione!\n"))
			return 0;

		len = ntohs(lenm);

		ret = recv(sd, (void *)buffer, len, 0);
		if (!check(ret, "Errore di comunicazione!\n"))
			return 0;

		if (strcmp(comando, "!esci") == 0) {
			strcpy(token, "");
		}

		printf("%s\n", (char *)buffer);
		
	}
}

int main(int argc, char **argv)
{

	/************ MODIFICA PER VELOCITÀ DI ESECUZIONE ************/

	if (argc == 1)
	{
		ipServer = "127.0.0.1";
		portaServer = 4242;
	}
	else

		/************ FINE MODIFICA *****************/

		if (argc != 3)
	{
		printf("\nATTENZIONE: Errato inserimento dei parametri\n");
		exit(-1);
	}
	else
	{
		//    printf("\nCorretto inserimento dei parametri\n\n");
		ipServer = argv[1];
		portaServer = atoi(argv[2]);
	}

	printInstruction();

	/* Creazione socket */
	sd = socket(AF_INET, SOCK_STREAM, 0);
	/* Creazione indirizzo */
	memset(&srv_addr, 0, sizeof(srv_addr)); // Pulizia
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(portaServer);
	inet_pton(AF_INET, ipServer, &srv_addr.sin_addr);
	ret = connect(sd, (struct sockaddr *)&srv_addr, sizeof(srv_addr));
	// Controlli sul successo della connect;

	if (sd < 0 || ret < 0)
	{
		printf("LOG_ERR: Errore di connessione, sd=%d, ret=%d \n", sd, ret);
		exit(-1);
	}


	while (1)
	{
		printf("\n%s", "> ");
		cleanBuf();

		fgets(buffer, MAX_BUF_LEN, stdin);
		sscanf(buffer, "%s %s %s", comando, parametro1, parametro2);
		printf("\n");

		if (!checkInstruction(comando))
		{
			continue;
		}

		if (strcmp(comando, "!help") == 0)
		{
			if (strcmp(parametro1, "") == 0)
			{
				printInstruction();
				continue;
			}
			else if (strcmp(parametro1, "!signup") == 0)
			{
				printf("Puoi utilizzare questo comando per registrarti, inserisci !signup <username> <password>\n");
				continue;
			}
			else if (strcmp(parametro1, "!login") == 0)
			{
				printf("Puoi utilizzare questo comando per il login, inserisci !login <username> <password>\n");
				continue;
			}
			else if (strcmp(parametro1, "!invia_giocata") == 0)
			{
				printf("Puoi utilizzare questo comando per inviare la giocata, ma attenzione alla sintassi, utilizza -r prima delle ruote, -n prima dei numeri e -i prima degli importi!\n ESEMPIO: !invia_giocata -r roma milano -n 15 19 33 -i 0 5 10\n ");
				continue;
			}
			else if (strcmp(parametro1, "!vedi_giocata") == 0)
			{
				printf("Puoi utilizzare questo comando per vedere le giocate: in particolare !vedi_giocata 0 per quelle passate; !vedi_giocata 1 per quelle in attesa\n");
				continue;
			}
			else if (strcmp(parametro1, "!vedi_estrazione") == 0)
			{
				printf("Puoi utilizzare questo comando per vedere le estrazioni, in particolare utilizza (!vedi_estrazione n ruota) per vedere le ultime n estrazioni sulla ruota specificata, non specificare la ruota per vedere i numeri estratti su tutte\n");
				continue;
			}
			else if (strcmp(parametro1, "!vedi_vincite") == 0)
			{
				printf("Puoi utilizzare questo comando per visualizzare le tue vincite!\n");
				continue;
			}
			else if (strcmp(parametro1, "!esci") == 0)
			{
				printf("Puoi utilizzare questo comando per uscire\n");
				continue;
			}
			else
			{
				printf("ATTENZIONE: Errata sintassi del comando !help, riprova stando più attento!\n");
				continue;
			}
		}

		if (strcmp(comando, "!signup") == 0)
		{
			if ((strcmp(parametro1, "") == 0) || (strcmp(parametro2, "") == 0))
			{
				printf("ATTENZIONE: Errata sintassi del comando !signup, riprova stando più attento!\n");
				continue;
			}
		}

		if (((strcmp(comando, "!signup") == 0) || (strcmp(comando, "!login") == 0)) && (strlen(token) != 0))
		{
			printf("ATTENZIONE: Hai già effettuato registrazione e login, prova ad utilizzare un altro comando\n");
			printf("token=%s\n", token);
			continue;
		}

		// check che l'utente stia seguendo le "regole"
		// check sulla correttezza del comando inserito;
		if ((strcmp(comando, "!invia_giocata") == 0) || (strcmp(comando, "!vedi_estrazioni") == 0))
		{
			if (strlen(token) == 0)
			{
				printf("ATTENZIONE: non puoi eseguire questo comando se non effettui prima il login!\n");
				continue;
			}
			else if ((strcmp(parametro1, "") == 0) || (strcmp(parametro2, "") == 0))
			{
				printf("ATTENZIONE: Errata sintassi del comando %s, riprova stando più attento!\n", comando);
				continue;
			}
		}

		if ((strcmp(comando, "!vedi_giocate") == 0) || (strcmp(comando, "!vedi_estrazioni") == 0))
		{
			if (strlen(token) == 0)
			{
				printf("ATTENZIONE: non puoi eseguire questo comando se non effettui prima il login!\n");
				continue;
			}
			else if (strcmp(parametro1, "") == 0)
			{
				printf("ATTENZIONE: Errata sintassi del comando %s, riprova stando più attento!\n", comando);
				continue;
			}
		}

		// procediamo all'invio dei dati al server;
		len = strlen(buffer) + 1;
		lenm = htons(len);

		ret = send(sd, (void *)&lenm, sizeof(uint16_t), 0);
		ret = send(sd, (void *)buffer, len, 0);

		strcpy(buffer, "");

		if (checkResponse(comando, parametro1, parametro2) == 0)
			continue;

		continue;
	}

	close(sd);
}
