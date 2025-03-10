#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TAMANHO_MAXIMO 5

typedef struct {
    char categoria[20];
    double limite_inferior;
    double limite_superior;
} Limite;

// Definição dos limites
Limite outliers[] = {
    {"Temperatura", 0, 100},
    {"PH", 3, 9},
    {"Turbidez", 0.1, 70},
    {"Condutividade", 10, 250},
    {"TDS", 10, 250}
};

// Vetor para armazenar os últimos valores
double valores[TAMANHO_MAXIMO] = {0};
int num_valores = 0;

// Função para obter os limites da categoria
Limite* obter_limites(const char* categoria) {
    for (int i = 0; i < sizeof(outliers) / sizeof(outliers[0]); i++) {
        if (strcmp(outliers[i].categoria, categoria) == 0) {
            return &outliers[i];
        }
    }
    return NULL;
}

// Função para calcular a média dos valores, excluindo o último
double calcular_media() {
    if (num_valores < 2) return valores[num_valores - 1];
    
    double soma = 0;
    for (int i = 0; i < num_valores - 1; i++) {
        soma += valores[i];
    }
    return round(soma / (num_valores - 1) * 100) / 100.0;
}

// Função para calcular o desvio padrão
double calcular_desvio_padrao() {
    if (num_valores < 2) return 0;
    
    double media = calcular_media();
    double soma = 0;
    for (int i = 0; i < num_valores - 1; i++) {
        soma += pow(valores[i] - media, 2);
    }
    return sqrt(soma / (num_valores - 1));
}

// Função para adicionar um novo valor
double adicionar_valor(double novo_valor, const char* categoria) {
    Limite* limites = obter_limites(categoria);
    if (!limites) {
        printf("Categoria não encontrada!\n");
        return 0;
    }
    
    // Desloca os valores para a esquerda se o vetor estiver cheio
    if (num_valores >= TAMANHO_MAXIMO) {
        for (int i = 0; i < TAMANHO_MAXIMO - 1; i++) {
            valores[i] = valores[i + 1];
        }
        num_valores--;
    }
    
    valores[num_valores++] = novo_valor;
    
    if (novo_valor < limites->limite_inferior || novo_valor > limites->limite_superior) {
        return valores[num_valores - 1] = calcular_media();
    }
    
    double media = calcular_media();
    double desvio_padrao = calcular_desvio_padrao();
    
    if (desvio_padrao > 0 && fabs(novo_valor - media) > 3 * desvio_padrao) {
        return valores[num_valores - 1] = calcular_media();
    }
    
    return round(novo_valor * 100) / 100.0;
}

int main() {
    double valores_entrada[] = {10, 20, 110, 30, 40, 5, 200};
    const char* categoria = "Temperatura";
    int n = sizeof(valores_entrada) / sizeof(valores_entrada[0]);
    
    for (int i = 0; i < n; i++) {
        printf("Entrada: %.2f, Saída: %.2f\n", valores_entrada[i], adicionar_valor(valores_entrada[i], categoria));
    }  
    return 0;
}
