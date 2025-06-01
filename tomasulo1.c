#include <stdio.h>
#include <string.h>

/* constantes de configuração */
#define MAX_RS 6   // Número de estações de reserva
#define MAX_REG 8   // Número de registradores

/* tipos de operações suportadas */
typedef enum {ADD, SUB, MUL, DIV, LOAD} OpType;

/* Estação de Reserva */
typedef struct {
    int busy;       // Indica se a RS está ocupada (1) ou livre (0)
    OpType op;      // Tipo de operação (ADD, SUB, etc.)
    float Vj, Vk;   // Valores dos operandos quando disponíveis
    int Qj, Qk;     // Tags das RS que estão produzindo os operandos (quando não disponíveis)
    int dest;       // Registrador destino
    int exec_cycles;// Ciclos restantes para conclusão
} RS;

/* Estado dos registradores */
typedef struct {
    float value;    // Valor atual do registrador
    int tag;        // Tag da RS que irá escrever neste registrador (0 quando pronto)
} RegStatus;

/* Variáveis globais do simulador */
RS rs[MAX_RS];                  // Todas as estações de reserva
RegStatus reg_status[MAX_REG];   // Status dos registradores
float regs[MAX_REG] = {0};       // Valores dos registradores
int cycle = 0;                   // Ciclo atual de simulação

/* Função auxiliar: converte enum de operação para string */
const char* op_str(OpType op) {
    static const char* names[] = {"ADD","SUB","MUL","DIV","LOAD"};
    return names[op];
}

/* Inicializa o simulador */
void init() {
    memset(rs, 0, sizeof(rs));               // Limpa todas as RS
    memset(reg_status, 0, sizeof(reg_status)); // Limpa status dos registradores
    
    // Inicializa registradores com valores padrão (R0=1, R1=2, etc.)
    for(int i=0; i<MAX_REG; i++) regs[i] = i+1; 
}

/* Emite uma instrução para as estações de reserva */
int issue(OpType op, int dest, int src1, int src2) {
    // Procura por uma RS livre
    for(int i=0; i<MAX_RS; i++) {
        if(!rs[i].busy) {
            rs[i].busy = 1;     // Marca como ocupada
            rs[i].op = op;      // Define a operação
            rs[i].dest = dest;  // Registrador destino
            
            /* Tratamento do operando 1 (Vj/Qj):
               Se o registrador fonte está pronto (tag=0), usa seu valor
               Senão, guarda a tag da RS que irá produzi-lo */
            rs[i].Vj = reg_status[src1].tag ? 0 : regs[src1];
            rs[i].Qj = reg_status[src1].tag;
            
            /* Para operações que usam dois operandos (tudo exceto LOAD) */
            if(op != LOAD) {
                rs[i].Vk = reg_status[src2].tag ? 0 : regs[src2];
                rs[i].Qk = reg_status[src2].tag;
            }
            
            /* Define a latência de execução:
               MULT/DIV: 3 ciclos, outras: 1 ciclo */
            rs[i].exec_cycles = (op==MUL||op==DIV) ? 3 : 1;
            
            /* Marca o registrador destino como "aguardando" esta RS */
            reg_status[dest].tag = i+1; // Tag = índice da RS + 1
            return 1; // Sucesso
        }
    }
    return 0; // Nenhuma RS livre
}

/* Executa um ciclo nas estações de reserva */
void execute() {
    for(int i=0; i<MAX_RS; i++) {
        /* Verifica se a RS está:
           - Ocupada
           - Com operandos prontos (Qj e Qk zerados)
           - Ainda precisa executar */
        if(rs[i].busy && !rs[i].Qj && !rs[i].Qk && rs[i].exec_cycles > 0) {
            rs[i].exec_cycles--; // Decrementa contador de ciclos
        }
    }
}

/* Escreve resultados no CDB (Common Data Bus) */
void writeback() {
    for(int i=0; i<MAX_RS; i++) {
        /* Verifica se a RS está pronta para escrever:
           - Ocupada
           - Terminou a execução (exec_cycles = 0) */
        if(rs[i].busy && !rs[i].exec_cycles) {
            float result = 0;
            
            /* Calcula o resultado conforme a operação */
            switch(rs[i].op) {
                case ADD: result = rs[i].Vj + rs[i].Vk; break;
                case SUB: result = rs[i].Vj - rs[i].Vk; break;
                case MUL: result = rs[i].Vj * rs[i].Vk; break;
                case DIV: result = rs[i].Vj / rs[i].Vk; break;
                case LOAD: result = rs[i].Vj + 100; // Simula acesso à memória
            }
            
            /* Escreve no registrador destino */
            regs[rs[i].dest] = result;
            reg_status[rs[i].dest].tag = 0; // Marca como pronto
            
            /* Broadcast: atualiza todas as RS que esperavam por este resultado */
            for(int j=0; j<MAX_RS; j++) {
                if(rs[j].Qj == i+1) { rs[j].Vj = result; rs[j].Qj = 0; }
                if(rs[j].Qk == i+1) { rs[j].Vk = result; rs[j].Qk = 0; }
            }
            
            rs[i].busy = 0; // Libera a RS
        }
    }
}

/* Mostra o estado atual do simulador */
void print_status() {
    printf("\nCiclo %d:\n", cycle);
    
    /* Mostra registradores: valor atual e tag (se estiver esperando) */
    printf("Regs: ");
    for(int i=0; i<MAX_REG; i++) 
        printf("R%d=%.1f(%d) ", i, regs[i], reg_status[i].tag);
    
    /* Mostra estações de reserva ocupadas */
    printf("\nRS: ");
    for(int i=0; i<MAX_RS; i++) {
        if(rs[i].busy) {
            printf("[%s R%d ", op_str(rs[i].op), rs[i].dest);
            if(rs[i].Qj) printf("Qj%d ", rs[i].Qj); // Mostra tag se operando não pronto
            else printf("Vj%.1f ", rs[i].Vj);       // Mostra valor se operando pronto
            if(rs[i].op != LOAD) { // LOAD só usa um operando
                if(rs[i].Qk) printf("Qk%d ", rs[i].Qk);
                else printf("Vk%.1f ", rs[i].Vk);
            }
            printf("C%d] ", rs[i].exec_cycles); // Ciclos restantes
        }
    }
    printf("\n");
}

/* Função principal */
int main() {
    init(); // Inicializa o simulador
    
    /* Programa de exemplo:
       R1 = R2 + R3
       R4 = R1 * R5  (depende da anterior)
       R6 = Mem[R0+100] */
    issue(ADD, 1, 2, 3);
    issue(MUL, 4, 1, 5);
    issue(LOAD, 6, 0, 0);
    
    /* Executa 10 ciclos */
    for(int i=0; i<10; i++) {
        print_status(); // Mostra estado
        execute();     // Executa operações
        writeback();   // Escreve resultados
        cycle++;       // Avança ciclo
    }
    
    return 0;
}