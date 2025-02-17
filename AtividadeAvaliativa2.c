#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_QUESTOES 30
#define CARGO_ALVO "1601"
#define MAX_CANDIDATOS 1000
#define NUM_VAGAS 20

typedef struct {
    char id[10];
    double nota_lp;
    double nota_ml;
    double nota_esp;
    double media_final;
} Candidato;

char gabarito[NUM_QUESTOES][2];
char respostas[MAX_CANDIDATOS][NUM_QUESTOES][2];
char candidatos[MAX_CANDIDATOS][10];
int total_candidatos = 0;
int size, rank;

void split(const char *str, char result[NUM_QUESTOES][2]) {
    char *token;
    char temp[300];
    strcpy(temp, str);
    token = strtok(temp, ",");
    int i = 0;
    while (token != NULL && i < NUM_QUESTOES) {
        strcpy(result[i], token);
        token = strtok(NULL, ",");
        i++;
    }
}

void carregar_gabarito() {
    FILE *file = fopen("./dados/gabarito.csv", "r");
    if (!file) {
        printf("Erro ao abrir gabarito.csv\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    char linha[300];
    fgets(linha, sizeof(linha), file);
    split(linha, gabarito);
    fclose(file);
}

void carregar_respostas() {
    FILE *file = fopen("./dados/respostas.csv", "r");
    if (!file) {
        printf("Erro ao abrir respostas.csv\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    char linha[400];
    while (fgets(linha, sizeof(linha), file)) {
        char id[10], cargo[5], questoes[300];
        sscanf(linha, "\"%[^\"]\",\"%[^\"]\",\"%[^\"]\"", id, cargo, questoes);

        if (strcmp(cargo, CARGO_ALVO) == 0) {
            strcpy(candidatos[total_candidatos], id);
            split(questoes, respostas[total_candidatos]);
            total_candidatos++;
        }
    }

    fclose(file);
}

void calcular_acertos(int *acertos) {
    memset(acertos, 0, sizeof(int) * NUM_QUESTOES);
    
    for (int i = 0; i < total_candidatos; i++) {
        for (int j = 0; j < NUM_QUESTOES; j++) {
            if (strcmp(respostas[i][j], gabarito[j]) == 0) {
                acertos[j]++;
            }
        }
    }
}

void calcular_pontuacoes(double *pontuacoes) {
    int acertos[NUM_QUESTOES];
    calcular_acertos(acertos);

    int max_acertos = 0;
    for (int i = 0; i < NUM_QUESTOES; i++) {
        if (acertos[i] > max_acertos) {
            max_acertos = acertos[i];
        }
    }

    double grau_dificuldade[NUM_QUESTOES];
    double soma_graus[3] = {0, 0, 0};

    for (int i = 0; i < NUM_QUESTOES; i++) {
        grau_dificuldade[i] = (double)max_acertos / acertos[i] * 4;
        if (i < 10) soma_graus[0] += grau_dificuldade[i];
        else if (i < 20) soma_graus[1] += grau_dificuldade[i];
        else soma_graus[2] += grau_dificuldade[i];
    }

    for (int i = 0; i < NUM_QUESTOES; i++) {
        int grupo = (i < 10) ? 0 : (i < 20) ? 1 : 2;
        pontuacoes[i] = (grau_dificuldade[i] / soma_graus[grupo]) * 100;
    }
}

void calcular_notas(Candidato *resultados, double *pontuacoes) {
    for (int i = 0; i < total_candidatos; i++) {
        resultados[i].nota_lp = 0;
        resultados[i].nota_ml = 0;
        resultados[i].nota_esp = 0;

        for (int j = 0; j < NUM_QUESTOES; j++) {
            if (strcmp(respostas[i][j], gabarito[j]) == 0) {
                if (j < 10) resultados[i].nota_lp += pontuacoes[j];
                else if (j < 20) resultados[i].nota_ml += pontuacoes[j];
                else resultados[i].nota_esp += pontuacoes[j];
            }
        }

        resultados[i].media_final = (resultados[i].nota_lp + resultados[i].nota_ml + resultados[i].nota_esp) / 3;
        strcpy(resultados[i].id, candidatos[i]);
    }
}

int comparar_candidatos(const void *a, const void *b) {
    Candidato *c1 = (Candidato *)a;
    Candidato *c2 = (Candidato *)b;
    return (c2->media_final - c1->media_final) > 0 ? 1 : -1;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    double tempo_inicio = MPI_Wtime(); // Início da medição

    if (rank == 0) {
        carregar_gabarito();
        carregar_respostas();
    }

    MPI_Bcast(gabarito, sizeof(gabarito), MPI_CHAR, 0, MPI_COMM_WORLD);
    MPI_Bcast(&total_candidatos, 1, MPI_INT, 0, MPI_COMM_WORLD);

    double pontuacoes[NUM_QUESTOES];
    calcular_pontuacoes(pontuacoes);

    Candidato resultados[MAX_CANDIDATOS];
    calcular_notas(resultados, pontuacoes);

    if (rank == 0) {
        qsort(resultados, total_candidatos, sizeof(Candidato), comparar_candidatos);
        
        // Cria o arquivo com a nota de cada questão
        FILE *arquivoNotaQuestao = fopen("./resultado/notaQuestao.csv", "w");
        if (!arquivoNotaQuestao) {
            printf("Erro ao criar o arquivo notaQuestao.csv\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        
        fprintf(arquivoNotaQuestao, "Questão,Nota\n");
        for (int i = 0; i < NUM_QUESTOES; i++) {
            fprintf(arquivoNotaQuestao, "%d,%.2f\n", i + 1, pontuacoes[i]);
        }
        fclose(arquivoNotaQuestao);

        // Cria o arquivo com a nota de cada candidato
        FILE *arquivoNotasCandidato = fopen("./resultado/notasCandidato.csv", "w");
        if (!arquivoNotasCandidato) {
            printf("Erro ao criar o arquivo notasCandidato.csv\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        
        fprintf(arquivoNotasCandidato, "IDCandidato,Nota_LP,Nota_ML,Nota_ESP,Media_Final\n");
        for (int i = 0; i < total_candidatos; i++) {
            fprintf(arquivoNotasCandidato, "%s,%.2f,%.2f,%.2f,%.2f\n",
                   resultados[i].id, resultados[i].nota_lp, resultados[i].nota_ml, resultados[i].nota_esp, resultados[i].media_final);
        }
        fclose(arquivoNotasCandidato);

        // Cria o arquivo apenas com os candidatos classificados
        FILE *arquivoClassificados = fopen("./resultado/classificados.csv", "w");
        if (!arquivoClassificados) {
            printf("Erro ao criar o arquivo classificados.csv\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        fprintf(arquivoClassificados, "IDCandidato,Media_Final\n");
        for (int i = 0; i < NUM_VAGAS; i++) {
            fprintf(arquivoClassificados, "%s,%.2f\n", resultados[i].id, resultados[i].media_final);
        }
        fclose(arquivoClassificados);

        double tempo_fim = MPI_Wtime(); // Fim da medição
        printf("\nTempo total de execução: %.6f segundos\n", tempo_fim - tempo_inicio);
    }

    MPI_Finalize();
    return 0;
}
