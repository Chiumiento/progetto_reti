#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>

#define MAX_BUF_LEN 1024

char pathFile[MAX_BUF_LEN] = "./Utenti/";
char ritorno[MAX_BUF_LEN];
char citta[12][MAX_BUF_LEN] = {{"bari"}, {"cagliari"}, {"firenze"}, {"genova"}, {"milano"}, {"napoli"}, {"palermo"}, {"roma"}, {"torino"}, {"venezia"}, {"nazionale"}, {"tutte"}};
char userLogged[MAX_BUF_LEN];
double importiVinti[5];
int tentativi = 3;
int blocco;

int ret, sd, new_sd, len, lenm;
struct sockaddr_in my_addr, client_addr;
pid_t pid;
char *buffer[1024];
int port;
int periodo = 5;
char cmd[1024];
uint16_t bufferSize;
const int numRuote = 11;
const int numPerRuota = 5;

struct estrazione
{
	int ruota;
	int numeri[5];
	int periodo;
};

struct estrazione extract[11];

struct estrazione letti[11];

struct giocata
{
	char ruoteGiocate[11][MAX_BUF_LEN];
	int numeriGiocati[10];
	double importiGiocati[5];
};
struct giocata g;

struct giocata *letta;

int isCittaCorretta(char *checkCitta)
{
	int i = 0;
	for (i = 0; i < 11; i++)
	{

		if ((strcmp(checkCitta, (char *)citta[i])) == 0)
		{
			//printf("LOG_DEBUG: checkCitta=%s, citta[i]=%s\n", checkCitta, citta[i]);
			return 1;
		}
	}
	return 0;
}

static inline int fattoriale(int n)
{
	if (n < 0)
		return -1;

	if (n == 0)
		return 1;
	else
		return n * fattoriale(n - 1);
}

static inline int combinazioni_semplici(int n, int k)
{
	int numeratore = fattoriale(n);
	int denominatore = fattoriale(n - k) * fattoriale(k);
	return numeratore / denominatore;
}

int getCittaId(char *checkCitta)
{
	int i = 0;
	for (i = 0; i < 11; i++)
	{

		if ((strcmp(checkCitta, (char *)citta[i])) == 0)
		{
			printf("LOG_DEBUG: checkCitta=%s, citta[i]=%s\n", checkCitta, citta[i]);
			return i;
		}
	}
	return -1;
}

void cleanBet()
{

	int i, j;
	for (i = 0; i < numRuote; i++)
	{
		extract[i].ruota = 0;
		extract[i].periodo = 0;
		for (j = 0; j < numPerRuota; j++)
			extract[i].numeri[j] = 0;
	}
}

int saveExtraction()
{

	int i, j;
	FILE *extrFile = fopen("Extrfile.txt", "ab+");

	if (extrFile == 0)
		return 0;

	for (i = 0; i < numRuote; i++)
	{
		fprintf(extrFile, " %d %s ", extract[0].periodo, citta[extract[i].ruota]);
		for (j = 0; j < numPerRuota; j++)
		{
			fprintf(extrFile, "%d ", extract[i].numeri[j]);
		}
		fprintf(extrFile, "\n");
	}

	fprintf(extrFile, "********************\n");
	fclose(extrFile);
	return 1;
}

void estrazione()
{

	int i, j, k;
	srand(time(NULL));

	for (i = 0; i < 11; i++)
	{
		extract[i].periodo = time(NULL);
		extract[i].ruota = i;
		for (j = 0; j < 5; j++)
		{
			extract[i].numeri[j] = rand() % 90 + 1;
			for (k = 0; k < j; k++)
			{
				while (extract[i].numeri[k] == extract[i].numeri[j])
				{
					extract[i].numeri[j] = rand() % 90 + 1;
				}
			}
		}
	}

	if (!saveExtraction())
		printf("ATTENZIONE: Errore nel salvataggio della corrente estrazione\n");
}

void start()
{

	while (1)
	{
		sleep(periodo);
		printf("LOG_INFO: Inizio estrazione\n");
		cleanBet();
		estrazione();
		printf("LOG_INFO: Fine estrazione\n");
	}
}

int check(int var, char *tipo)
{

	if (var < 0)
	{
		printf("LOG_ERROR: La funzione %s ha causato errore\n", tipo);
		return 0;
	}

	printf("LOG_INFO: %s eseguita correttamente\n", tipo);
	return 1;
}

char *findUserFile(char *username)
{

	strcpy(ritorno, pathFile);
	strcat(ritorno, username);
	strcat(ritorno, ".txt");
	return ritorno;
}

int isFree(char *username)
{

	char user[MAX_BUF_LEN];
	strcpy(user, findUserFile(username));
	FILE *check = fopen(user, "rb");
	if (!check)
		return 1;
	return 0;
}

int signup(char *username, char *password)
{

	if (!isFree(username))
		return 0;
	char *file = findUserFile(username);

	FILE *regFile = fopen(file, "ab+");

	fprintf(regFile, "%s %s\n", username, password);
	fclose(regFile);
	return 1;
}

void sendResponse(int risposta, char *response)

{
	printf("LOG_DEBUG: START sendResponse -> risposta=%d, response=%s\n", risposta, response);
	ret = send(new_sd, (void *)&risposta, sizeof(uint16_t), 0);

	if (!check(ret, "send"))
		return;

	if (strlen(response) != 0)
	{
		printf("LOG_DEBUG: sending response=%s\n", response);
		len = strlen(response) + 1;
		lenm = htons(len);

		ret = send(new_sd, (void *)&lenm, sizeof(uint16_t), 0);
		if (!check(ret, "send_len"))
			return;
		ret = send(new_sd, (void *)response, len, 0);
		if (!check(ret, "send_buffer"))
			return;
	}
	return;
}

void login(char *username, char *password)
{

	char file[MAX_BUF_LEN];
	char user[MAX_BUF_LEN];
	char pass[MAX_BUF_LEN];
	char token[10];
	strcpy(file, findUserFile(username));
	FILE *userFile = fopen(file, "rb");

	printf("LOG_DEBUG: username=%s\n", username);
	srand(time(0));

	if (userFile == NULL)
	{
		perror("LOG_ERROR: Username non registrato, errore aprendo il file\n");
		tentativi--;
		if (tentativi == 0)
		{
			blocco = time(NULL) + 30;
			sendResponse(0, "Tentativi esauriti! Attendere 30 minuti!");
		}
		else
		{
			sendResponse(0, "USERNAME NON REGISTRATO");
		}
		return;
	}

	strcpy(file, "");
	char *str = fgets(file, MAX_BUF_LEN, userFile);

	if (str == NULL)
	{
		perror("LOG_ERROR: Username non registrato\n");

		tentativi--;
		if (tentativi == 0)
		{
			sendResponse(0, "Tentativi esauriti! Attendere 30 minuti!");
		}
		else
		{
			blocco = time(NULL) + 30;
			sendResponse(0, "USERNAME NON REGISTRATO");
		}
		return;
	}

	sscanf(str, "%s %s", user, pass);

	if (strcmp(password, pass) == 0)
	{
		int i = 0;
		while (i < 3)
		{
			token[i] = 48 + (rand() % 10);
			i++;
		}

		while (i < 10)
		{

			token[i] = 97 + rand() % 26;
			i++;
		}

		token[10] = '\0';
		strcpy(userLogged, username);

		sendResponse(1, token);
	}
	else
	{
		printf("LOG_WARN: Password Errata\n");
		tentativi--;
		if (tentativi == 0)
		{
			sendResponse(0, "Tentativi esauriti! Attendere 30 minuti!");
		}
		else
		{
			blocco = time(NULL) + 30;
			sendResponse(0, "Password Errata");
		}
	}
}

static inline void initSchedina()
{
	memset(g.numeriGiocati, 0, 10 * sizeof(int));
	memset(g.importiGiocati, 0, 5 * sizeof(double));
}

void stampa_schedina_su_file(char *log)
{
	FILE *registroLoggato = fopen(log, "ab+");

	int i = 0;

	printf("LOG_INFO: START - Stampo schedina su file\n");

	for (i = 0; i < 11; i++)
	{

		printf("LOG_INFO: g.ruoteGiocate=%s\n", g.ruoteGiocate[i]);
		if (strcmp(g.ruoteGiocate[i], "") != 0)
			fprintf(registroLoggato, "%s ", g.ruoteGiocate[i]);
		else
			fprintf(registroLoggato, "%s ", "-");
	}

	for (i = 0; i < 10; i++)
	{
		if (g.numeriGiocati[i] != 0)
		{
			printf("LOG_INFO: g.numeriGiocati=%d\n", g.numeriGiocati[i]);
			fprintf(registroLoggato, "%d ", g.numeriGiocati[i]);
		}
		else
		{
			fprintf(registroLoggato, "%s ", "-");
		}
	}

	for (i = 0; i < 5; i++)
	{
		if (g.importiGiocati[i] != 0)
			fprintf(registroLoggato, "%f ", g.importiGiocati[i]);
		else
			fprintf(registroLoggato, "%s ", "-");
	}

	fprintf(registroLoggato, "t: %d", (int)time(NULL));

	fprintf(registroLoggato, "\n");
	fclose(registroLoggato);
}

char *visualizza_schedine(int tipo, int showTS)
{
	char *str = malloc(sizeof(char) * MAX_BUF_LEN);
	char *ret = malloc(sizeof(char) * MAX_BUF_LEN);
	FILE *estrazioni = fopen("Extrfile.txt", "r");
	FILE *userFile = NULL;
	char ultimoTS[MAX_BUF_LEN];
	char linea[MAX_BUF_LEN];
	int i = 0;

	if (estrazioni == NULL)
	{
		printf("LOG_ERROR: Ancora non ho avuto estrazioni\n");
		return "NESSUNA ESTRAZIONE DA VISUALIZZARE";
	}

	strcpy(ultimoTS, "");

	while (1)
	{
		char numero1[MAX_BUF_LEN];
		char numero2[MAX_BUF_LEN];
		char numero3[MAX_BUF_LEN];
		char numero4[MAX_BUF_LEN];
		char numero5[MAX_BUF_LEN];
		char ruota[MAX_BUF_LEN];

		fscanf(estrazioni, "%s %s %s %s %s %s %s ", ultimoTS, ruota, numero1, numero2, numero3, numero4, numero5);
		if (feof(estrazioni))
		{
			break;
		}

		if (i == 10)
		{
			fscanf(estrazioni, "%s ", ruota);
			i = 0;
		}
		else
		{
			i++;
		}
	}
	printf("LOG_DEBUG: ultimoTS=%s", ultimoTS);
	fclose(estrazioni);

	userFile = fopen(findUserFile(userLogged), "rb");
	strcpy(str, "");
	strcpy(ret, "");
	fscanf(userFile, "%s ", linea);
	fscanf(userFile, "%s ", linea);
	if (feof(userFile))
	{
		strcpy(ret, "NESSUNA GIOCATA");
		return ret;
	}
	while (1)
	{
		char ts[MAX_BUF_LEN];
		strcpy(str, "");

		if (feof(estrazioni))
		{
			break;
		}

		strcpy(linea, "");
		for (i = 0; i < 26; i++)
		{
			char app0[MAX_BUF_LEN];
			fscanf(userFile, "%s ", app0);
			if (linea == NULL)
			{
				strcat(linea, " ");
				strcpy(linea, app0);
			}
			else
			{
				strcat(linea, " ");
				strcat(linea, app0);
			}
		}

		fscanf(userFile, "%s ", ts);
		if (strcmp(ts, "t:") == 0)
		{
			int timestamp;
			fscanf(userFile, "%s ", ts);
			printf("LOG_DEBUG: Timestamp=%s, ultimoTS=%d\n", ts, atoi(ultimoTS));
			timestamp = atoi(ts);
			if (((timestamp > atoi(ultimoTS)) && tipo == 1) || ((timestamp < atoi(ultimoTS)) && tipo == 0))
			{
				strcpy(str, linea);
				if (showTS == 1)
				{
					char app0[MAX_BUF_LEN];
					strcpy(app0, "");
					strcpy(app0, str);
					strcpy(str, ts);
					strcat(str, app0);
				}
			}
		}
		if (ret == NULL)
		{
			strcat(str, "\n");
			strcpy(ret, str);
		}
		else
		{
			strcat(str, "\n");
			strcat(ret, str);
		}
		printf("LOG_DEBUG: str=%s\n", str);
		printf("LOG_DEBUG: ret=%s\n", ret);
		strcpy(str, "");
	}

	fclose(userFile);
	return ret;
}
char *leggi_estrazioni_blank(char *quante)
{
	char *str_to_ret = malloc(sizeof(char) * MAX_BUF_LEN);
	char *ritorno = malloc(sizeof(char) * 10000);

	int q = atoi(quante);
	int i = 0;

	FILE *estrazioni = fopen("Extrfile.txt", "r");

	if (estrazioni == NULL)
	{
		printf("LOG_ERROR: Ancora non ho avuto estrazioni\n");
		return "NESSUNA ESTRAZIONE DA VISUALIZZARE";
	}

	char linea[MAX_BUF_LEN];
	strcpy(linea, "");

	fscanf(estrazioni, "%s ", linea);
	printf("LOG_DEBUG: Linea %s\n", linea);
	strcpy(str_to_ret, "");
	strcpy(ritorno, "");
	while (i < q)
	{
		strcpy(str_to_ret, "");

		if (feof(estrazioni))
		{
			break;
		}

		if (strstr(linea, "***") != NULL)
		{
			i++;
			fscanf(estrazioni, "%s ", linea);
			continue;
		}

		if (strstr(linea, "nazionale") != NULL)
		{
			i++;
		}

		printf("LOG_TRACE: linea=%s\n", linea);
		if (isCittaCorretta(linea) == 0)
		{
			fscanf(estrazioni, "%s ", linea);
			continue;
		}

		int j = 0;
		strcpy(str_to_ret, linea);
		for (j = 0; j < 6; j++)
		{
			fscanf(estrazioni, "%s ", linea);
			if (strlen(linea) < 4)
			{
				strcat(str_to_ret, " ");
				strcat(str_to_ret, linea);
			}
		}

		printf("LOG_TRACE: str_to_ret=%s, ritorno=%s", str_to_ret, ritorno);
		if (str_to_ret != NULL && strcmp(str_to_ret, "") != 0)
		{
			strcat(str_to_ret, "\n");
			if (ritorno == NULL)
			{
				strcpy(ritorno, str_to_ret);
			}
			else
			{
				strcat(ritorno, str_to_ret);
			}
		}
		strcpy(str_to_ret, "");
		fscanf(estrazioni, "%s ", linea);

		if (strstr(linea, "***") != NULL)
		{
			i++;
			fscanf(estrazioni, "%s ", linea);
			continue;
		}
	}
	printf("LOG_TRACE: i=%d", i);
	fclose(estrazioni);
	return ritorno;
}

char *leggi_estrazioni(char *quante, char *ruota)
{
	char *str_to_ret = malloc(sizeof(char) * MAX_BUF_LEN);
	char *ritorno = malloc(sizeof(char) * 10000);

	int q = atoi(quante);
	int i = 0;

	FILE *estrazioni = fopen("Extrfile.txt", "r");

	if (estrazioni == NULL)
	{
		printf("LOG_ERROR: Ancora non ho avuto estrazioni\n");
		return "NESSUNA ESTRAZIONE DA VISUALIZZARE";
	}

	char linea[MAX_BUF_LEN];
	strcpy(linea, "");

	fscanf(estrazioni, "%s ", linea);
	printf("LOG_DEBUG: Linea %s\n", linea);
	strcpy(str_to_ret, "");
	strcpy(ritorno, "");
	while (i < q)
	{
		strcpy(str_to_ret, "");
		printf("LOG_DEBUG: RUOTA=%s\n", ruota);
		if (feof(estrazioni))
		{
			break;
		}

		if (strstr(linea, "***") != NULL)
		{
			i++;
			fscanf(estrazioni, "%s ", linea);
			continue;
		}

		printf("LOG_TRACE: linea=%s\n", linea);
		if (isCittaCorretta(linea) == 0)
		{
			fscanf(estrazioni, "%s ", linea);
			continue;
		}

		if (strcmp(linea, ruota) == 0 || strlen(ruota) == 0)
		{
			int j = 0;
			printf("LOG_DEBUG: Città corretta! linea=%s, citta=%s", linea, ruota);
			strcpy(str_to_ret, linea);
			for (j = 0; j < 6; j++)
			{
				fscanf(estrazioni, "%s ", linea);
				if (strlen(linea) < 4)
				{
					strcat(str_to_ret, " ");
					strcat(str_to_ret, linea);
				}
			}
		}

		printf("LOG_TRACE: str_to_ret=%s, ritorno=%s", str_to_ret, ritorno);
		if (str_to_ret != NULL && strcmp(str_to_ret, "") != 0)
		{
			strcat(str_to_ret, "\n");
			if (ritorno == NULL)
			{
				strcpy(ritorno, str_to_ret);
			}
			else
			{
				strcat(ritorno, str_to_ret);
			}
		}
		strcpy(str_to_ret, "");
		fscanf(estrazioni, "%s ", linea);

		if (strstr(linea, "***") != NULL)
		{
			i++;
			fscanf(estrazioni, "%s ", linea);
			continue;
		}
	}
	printf("LOG_TRACE: i=%d", i);
	fclose(estrazioni);
	return ritorno;
}

char *mesi[] = {"Gennaio", "Febbraio", "Marzo", "Aprile", "Maggio", "Giugno",
				"Luglio", "Agosto", "Settembre", "Ottobre", "Novembre", "Dicembre"};

char *mostra_vincite()
{
	char *output = malloc(sizeof(char) * MAX_BUF_LEN);
	char schedina[MAX_BUF_LEN];
	char *app;
	char ultimoTS[MAX_BUF_LEN];
	char ruota[MAX_BUF_LEN];
	int numeri_giocati[10];
	int importi[5];
	int check = 0;
	int giocati = 0;
	FILE *estrazioni = fopen("Extrfile.txt", "r");
	strcpy(schedina, visualizza_schedine(0, 1));
	strcpy(output, "");
	strcpy(ruota, "");

	printf("LOG_TRACE: schedina=%s", schedina);

	if (strlen(schedina) == 0)
	{
		strcpy(output, "NESSUNA VINCITA");
		printf("LOG_DEBUG: NESSUNA VINCITA");
		return output;
	}

	int i = 0;

	if (estrazioni == NULL)
	{
		printf("LOG_ERROR: Ancora non ho avuto estrazioni\n");
		return "NESSUNA ESTRAZIONE DA VISUALIZZARE";
	}

	strcpy(ultimoTS, "");
	app = strtok((char *)schedina, " ");
	printf("LOG_TRACE: app=%s", app);
	app = strtok(NULL, " ");
	printf("LOG_TRACE: app=%s", app);
	strcpy(ruota, app);
	while (app != NULL)
	{
		app = strtok(NULL, " ");
		if (app == NULL)
			break;
		if (strcmp(app, "-") != 0 && i < 9)
		{
			printf("LOG_DEBUG: app=%s\n", app);
		}
		else if (strcmp(app, "-") != 0 && i < 19)
		{
			printf("LOG_DEBUG: numero app=%s\n", app);
			numeri_giocati[i - 10] = atoi(app);
			giocati++;
		}
		else if (strcmp(app, "-") != 0)
		{
			printf("LOG_DEBUG: importo app=%s\n", app);
			importi[i - 20] = atoi(app);
		}

		/* 		if (strcmp(app, "-") != 0 && i < 10) {
			numeri_giocati[i] = atoi(app);
			printf("LOG_DEBUG: atoi(app)=%d", atoi(app));
			i++;
		} else if (strcmp(app, "-") != 0) {
			importi[i] = atoi(app);
			printf("LOG_DEBUG: atoi(app)=%d", atoi(app));
			i++;
		} */
		i++;
	}
	printf("numeri_giocati=%d\n", importi[0]);
	printf("importi=%d\n", numeri_giocati[0]);
	i = 0;
	while (1)
	{
		char numero1[MAX_BUF_LEN];
		char numero2[MAX_BUF_LEN];
		char numero3[MAX_BUF_LEN];
		char numero4[MAX_BUF_LEN];
		char numero5[MAX_BUF_LEN];
		char ruota_rel[MAX_BUF_LEN];

		fscanf(estrazioni, "%s %s %s %s %s %s %s ", ultimoTS, ruota_rel, numero1, numero2, numero3, numero4, numero5);

		if ((atoi(ultimoTS) > atoi(schedina)) && strcmp(ruota, ruota_rel) == 0)
		{
			struct tm *leggibile;
			time_t rawtime = atoi(ultimoTS);
			leggibile = localtime(&rawtime);
			char data[MAX_BUF_LEN];
			sprintf(data, "%d %s %d ore %d:%d", leggibile->tm_mday, mesi[leggibile->tm_mon], leggibile->tm_year + 1900, leggibile->tm_hour, leggibile->tm_min);
			sprintf(output, "Estrazione del %s\n", data);
			printf("%s\n", output);
			printf("LOG_TRACE: schedina_associata %s %s %s %s %s %s %s ", ultimoTS, ruota_rel, numero1, numero2, numero3, numero4, numero5);
			for (i = 0; i < 10; i++)
			{
				if (numeri_giocati[i] == atoi(numero1) || numeri_giocati[i] == atoi(numero2) || numeri_giocati[i] == atoi(numero3) || numeri_giocati[i] == atoi(numero4) || numeri_giocati[i] == atoi(numero5))
					check++;
			}
			break;
		}

		if (feof(estrazioni))
		{
			break;
		}

		if (i == 10)
		{
			fscanf(estrazioni, "%s ", ruota_rel);
			i = 0;
		}
		else
		{
			i++;
		}
	}
	//n!/(n-k)!k!
	if (check > 0)
	{
		if (importi[0] != 0)
		{
			printf("LOG_DEBUG: importi[0]=%d", importi[0]);
			double vittoria = importi[0] * check * 11.23 / combinazioni_semplici(giocati, 1);
			char rit[10000];
			sprintf(rit, "%s singolo %f", ruota, vittoria);
			strcat(output, rit);
		}
		if (importi[1] != 0 && check >= 2)
		{
			printf("LOG_DEBUG: importi[1]=%d", importi[1]);
			double vittoria = importi[1] * check * 250 / combinazioni_semplici(giocati, 2);
			char rit[10000];
			sprintf(rit, "%s ambo %f", ruota, vittoria);
			strcat(output, rit);
		}
		if (importi[2] != 0 && check >= 3)
		{
			printf("LOG_DEBUG: importi[2]=%d", importi[2]);
			double vittoria = importi[2] * check * 4500 / combinazioni_semplici(giocati, 3);
			char rit[10000];
			sprintf(rit, "%s terno %f", ruota, vittoria);
			strcat(output, rit);
		}
		if (importi[3] != 0 && check >= 4)
		{
			printf("LOG_DEBUG: importi[3]=%d", importi[0]);
			double vittoria = importi[3] * check * 120000 / combinazioni_semplici(giocati, 4);
			char rit[10000];
			sprintf(rit, "%s quaterna %f", ruota, vittoria);
			strcat(output, rit);
		}
		if (importi[4] != 0 && check >= 5)
		{
			printf("LOG_DEBUG: importi[4]=%d", importi[4]);
			double vittoria = importi[4] * check * 6000000 / combinazioni_semplici(giocati, 5);
			char rit[10000];
			sprintf(rit, "%s cinquina %f", ruota, vittoria);
			strcat(output, rit);
		}
	}
	else
	{
		strcpy(output, "NESSUNA VINCITA");
	}
	printf("LOG_DEBUG: ultimoTS=%s", ultimoTS);
	printf("LOG_INFO: numeri presi=%d", check);
	fclose(estrazioni);
	strcat(output, "\n");
	return output;
}

int main(int argc, char **argv)
{

	/************ MODIFICA PER VELOCITÀ DI ESECUZIONE ************/

	if (argc == 1)
	{
		port = 4242;
	}
	else

		/************ FINE MODIFICA *****************/

		port = atoi(argv[1]);

	if (argc == 3)
	{
		periodo = atoi(argv[2]);
	}

	printf("LOG_INFO periodo=%d porta=%d \n", periodo, port);

	/* Creazione socket */
	sd = socket(AF_INET, SOCK_STREAM, 0);

	if (!check(sd, "socket"))
		exit(-1);

	/* Creazione indirizzo */
	memset(&my_addr, 0, sizeof(my_addr)); // Pulizia;
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(port);
	my_addr.sin_addr.s_addr = INADDR_ANY;

	// Riutilizzo porta (REUSEADDR);
	int yes = 1;
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1)
	{
		perror("setsockopt");
		exit(-1);
	}

	ret = bind(sd, (struct sockaddr *)&my_addr, sizeof(my_addr));
	if (!check(ret, "bind"))
		exit(-1);

	ret = listen(sd, 10);
	if (!check(ret, "listen"))
		exit(-1);

	while (1)
	{
		len = sizeof(client_addr);
		new_sd = accept(sd, (struct sockaddr *)&client_addr, &len);
		if (!check(new_sd, "accept"))
			exit(-1);

		/***** INSERIRE CONTROLLO IP ******/

		pid = fork();
		if (pid == 0)
		{

			// chiusura del socket sd, siamo nel figlio che non lo usa
			int closeSD = close(sd);
			if (closeSD < 0)
			{
				printf("LOG_ERROR: Errore in fase di chiusura del socket\n");
				exit(-1);
			}
			else
			{
				printf("LOG_INFO: Socket chiuso correttamente\n");
			}

			while (1)
			{

				pid_t gestoreEstrazione = fork();
				if (gestoreEstrazione == 0)
				{
					// mi serve un processo che gestisca l'estrazione
					start();
				}
				else
				{
					char buffer[MAX_BUF_LEN];
					char comando[MAX_BUF_LEN];
					char parametro1[MAX_BUF_LEN];
					char parametro2[MAX_BUF_LEN];
					char response[MAX_BUF_LEN];
					int risposta;

					strcpy(buffer, "");
					strcpy(response, "");

					ret = recv(new_sd, (void *)&lenm, sizeof(uint16_t), 0);
					if (!check(ret, "receive"))
						break;

					len = ntohs(lenm);
					ret = recv(new_sd, (void *)buffer, len, 0);
					if (!check(ret, "receive"))
						break;
					printf("LOG_INFO: Messaggio ricevuto correttamente dal client\n");

					sscanf((char *)buffer, "%s", comando);

					if (strcmp(comando, "!signup") == 0)
					{
						printf("LOG_INFO: È arrivato il comando !signup\n");
						sscanf(buffer, "%s %s %s", comando, parametro1, parametro2);

						if (!signup(parametro1, parametro2))
						{
							strcpy(response, "Username_già_registrato");
							printf("LOG_wARN: %s\n", response);
							risposta = 0;
						}
						else
						{
							printf("LOG_INFO: Registrazione avvenuta con successo\n");
							risposta = 1;
						}

						sendResponse(risposta, response);

						continue;
					}
					else if (strcmp(comando, "!login") == 0)
					{
						if (tentativi <= 0 && time(NULL) <= blocco)
						{
							sendResponse(0, "Impossibile acceder, IP BLOCCATO");
							continue;
						}
						else if (tentativi <= 0 && time(NULL) > blocco)
						{
							tentativi = 3;
						}
						strcpy(parametro1, "");
						strcpy(parametro2, "");

						sscanf(buffer, "%s %s %s", comando, parametro1, parametro2);

						login(parametro1, parametro2);

						continue;
					}
					else if (strcmp(comando, "!invia_giocata") == 0)
					{
						printf("LOG_INFO: Invia_giocata: buffer=%s\n", buffer);
						char *option;
						int i = 0;
						option = strtok((char *)buffer, " ");
						initSchedina();

						if (option != NULL)
						{
							option = strtok(NULL, " ");
							printf("LOG_DEBUG: option=%s\n", option);

							if (strcmp(option, "-r") == 0)
							{

								while (strcmp(option, "-n") != 0 && i < 11)
								{

									option = strtok(NULL, " ");
									printf("LOG_DEBUG: i=%d, option=%s\n", i, option);

									if (strcmp(option, "-n") == 0)
										break;

									if (i < 11 && isCittaCorretta(option) == 1)
									{
										strcpy(g.ruoteGiocate[i], option);
										printf("LOG_DEBUG: i=%d, g.ruoteGiocate=%s, option=%s\n", i, g.ruoteGiocate[i], option);
										i++;
									}
									else
									{
										printf("LOG_DEBUG: Città non corretta\n");
										sendResponse(0, "Citta_non_corretta/e");
										break;
									}

									if (g.ruoteGiocate[i - 1] == "")
									{
										sendResponse(0, "Città_non_inserite_correttamente\n");
										printf("LOG_ERROR: Errore nell'inserimento delle città\n");
										break;
									}

									printf("LOG_INFO: Ruota: %s, numero: %s\n", option, g.ruoteGiocate[i - 1]);
								}
							}
							else
							{
								sendResponse(0, "Inserimento_giocata_fallito");
								printf("LOG_ERROR: Inserimento giocata fallito\n");
								continue;
							}

							i = 0;

							if (strcmp(option, "-n") == 0)
							{
								printf("LOG_DEBUG: option=%s\n", option);
								while (strcmp(option, "-i") != 0)
								{
									option = strtok(NULL, " ");
									printf("LOG_DEBUG: option=%s\n", option);

									if (strcmp(option, "-i") == 0)
									{
										break;
									}

									int numero = atoi(option);

									printf("LOG_INFO: Valore giocata = %d\n", numero);

									if ((numero <= 0) || (numero > 90))
									{
										printf("LOG_ERROR: Valore giocata non valido\n");
										sendResponse(0, "Valore_giocata_non_valido");
										break;
									}

									if (i < 10)
									{
										g.numeriGiocati[i++] = numero;
									}
									else
									{
										printf("LOG_DEBUG: Raggiunto il numero massimo di numeri giocati\n");
										sendResponse(0, "Troppi numeri");
										break;
									}
								}
							}
							else
							{
								printf("LOG_ERROR: Errore nell'invio della giocata");
							//	sendResponse(0, "Errore_invio_giocata");
								continue;
							}
							i = 0;

							if (strcmp(option, "-i") == 0)
							{
								printf("LOG_DEBUG: option=%s\n", option);
								while (option != NULL)
								{
									option = strtok(NULL, " ");
									if (option == NULL)
									{
										break;
									}
									double importo = atof(option);
									printf("LOG_DEBUG: Importo: %f\n", importo);

									if (i < 5)
										g.importiGiocati[i++] = importo;
									else
										printf("LOG_WARN: Importo esaurito\n");

									if ((g.importiGiocati[i - 1] < 0))
									{
										printf("LOG_ERROR: Importo non valido\n");
										sendResponse(0, "Importo_non_valido");
										break;
									}
								}
							}
							else
							{
								printf("LOG_ERROR: Formato importo sbagliato\n");
							//	sendResponse(0, "Formato_non_corretto");
								continue;
							}
						}
						else
						{
							printf("LOG_ERROR: !invia_giocata vuota\n");
							sendResponse(0, "Giocata_vuota");
							continue;
						}
						printf("LOG_TRACE: user=%s", userLogged);
						char *log = findUserFile(userLogged);

						printf("LOG_DEBUG: pathFile=%s", log);

						stampa_schedina_su_file(log);

						sendResponse(0, "GIOCATA INSERITA CORRETTAMENTE");
						printf("LOG_DEBUG: END - !invia_giocata\n");
						continue;
					}
					else if (strcmp(comando, "!vedi_giocate") == 0)
					{
						printf("LOG_INFO: Vedi_giocata: buffer=%s\n", buffer);

						strcpy(parametro1, "");

						sscanf(buffer, "%s %s", comando, parametro1);

						int tipo = atoi(parametro1);

						printf("LOG_DEBUG: Tipo di giocate da mostrare: %d\n", tipo);

						if (tipo != 0 && tipo != 1)
						{
							printf("LOG_ERROR: Tipo errato\n");
							sendResponse(0, "Tipo_non_valido");
							continue;
						}

						char gioco[MAX_BUF_LEN];
						strcpy(gioco, visualizza_schedine(tipo, 0));
						printf("LOG_TRACE: gioco=%s", gioco);
						printf("LOG_INFO: Listato delle giocate: %s\n", gioco);

						if (strlen(gioco) != 0 && gioco != NULL)
						{
							sendResponse(0, gioco);
						}
						else
						{
							sendResponse(0, "NESSUNA ESTRAZIONE");
						}
						continue;
					}
					else if (strcmp(comando, "!vedi_estrazione") == 0)
					{

						printf("LOG_INFO: START - !vedi_estrazione\n");
						strcpy(parametro1, "");
						strcpy(parametro2, "");
						sscanf(buffer, "%s %s %s", comando, parametro1, parametro2);

						char rit[MAX_BUF_LEN];

						if (strlen(parametro2) == 0)
						{
							strcpy(rit, leggi_estrazioni_blank(parametro1));
						}
						else
						{
							strcpy(rit, leggi_estrazioni(parametro1, parametro2));
						}
						printf("LOG_TRACE: rit=%s\n", rit);
						sendResponse(0, rit);
					}
					else if (strcmp(comando, "!vedi_vincite") == 0)
					{
						printf("LOG_INFO: START - !vedi_vincite\n");

						char *show_win = mostra_vincite();

						if (strlen(show_win) != 0 && show_win != NULL)
						{
							sendResponse(0, show_win);
						}
						else
						{
							sendResponse(0, "NESSUNA VINCITA");
						}
						continue;
					}
					else if (strcmp(comando, "!esci") == 0)
					{
						printf("LOG_INFO: START - !esci\n");
						printf("LOG_INFO: Logout effettuato con successo\n");
						strcpy(userLogged, "");

						sendResponse(0, "Logout effettuato con successo");
					}

					continue;
				}
			}
		}
		else
			close(new_sd);
	}
}
