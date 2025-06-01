#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <string>
#include <unordered_map>
#include <iomanip>

using namespace std;

// Estrutura para representar uma instrução
struct Instrucao {
    string opcode;
    string rd, rs, rt;
};

// Estrutura para representar uma estação de reserva
struct EstacaoDeReserva {
    string nome;
    bool ocupado;
    string op;
    string Vj, Vk;
    string Qj, Qk;
    int ciclosRestantes;
    string destino;
    int cicloEmissao;
    int cicloExecucaoInicio;
    int cicloExecucaoFim;
    int cicloWriteBack;
};

// Estrutura para representar um registrador
struct Registrador {
    string Qi; // Tag da estação de reserva que irá escrever neste registrador
    int valor;
};

class SimuladorTomasulo {
private:
    int ciclo;
    queue<Instrucao> filaInstrucoes;
    vector<EstacaoDeReserva> estacoesAdd;
    vector<EstacaoDeReserva> estacoesMul;
    unordered_map<string, Registrador> registradores;
    vector<string> cdb; // Barramento de Dados Comum
    vector<EstacaoDeReserva*> estacoesAtivas; // Estações que estão em execução

    // Latências das operações
    const int latenciaAddSub = 2; // Ciclos para ADD e SUB
    const int latenciaMul = 10;   // Ciclos para MUL
    const int latenciaDiv = 40;   // Ciclos para DIV

    // Limite de escrita no CDB (apenas uma operação pode escrever por ciclo)
    const int limiteCDB = 1;

public:
    SimuladorTomasulo(queue<Instrucao> filaInstr) : filaInstrucoes(filaInstr), ciclo(0) {
        // Inicializa estações de reserva
        for (int i = 0; i < 3; i++) {
            estacoesAdd.push_back({"Add" + to_string(i + 1), false, "", "", "", "", "", 0, "", 0, 0, 0, 0});
        }
        for (int i = 0; i < 2; i++) {
            estacoesMul.push_back({"Mul" + to_string(i + 1), false, "", "", "", "", "", 0, "", 0, 0, 0, 0});
        }
        // Inicializa registradores com valores arbitrários para demonstração
        for (int i = 0; i <= 10; i++) {
            registradores["R" + to_string(i)] = {"", i + 1}; // Registradores R0 to R10
        }
    }

    void executar() {
        cout << setw(10) << "Ciclo" << setw(20) << "Evento" << endl;
        cout << "----------------------------------------" << endl;
        while (!filaInstrucoes.empty() || estaOcupado() || !estacoesAtivas.empty()) {
            ciclo++;
            cout << setw(10) << ciclo << endl;
            escreverResultado();
            executarInstrucoes();
            emitir();
            imprimirStatus();
            cout << "----------------------------------------" << endl;
        }
    }

    void emitir() {
        if (filaInstrucoes.empty()) return;
        Instrucao instr = filaInstrucoes.front();
        vector<EstacaoDeReserva>* estacoes;
        int ciclosExecucao;
        if (instr.opcode == "ADD" || instr.opcode == "SUB") {
            estacoes = &estacoesAdd;
            ciclosExecucao = latenciaAddSub;
        } else if (instr.opcode == "MUL" || instr.opcode == "DIV") {
            estacoes = &estacoesMul;
            if (instr.opcode == "MUL")
                ciclosExecucao = latenciaMul;
            else
                ciclosExecucao = latenciaDiv;
        } else {
            cout << "Instrução não suportada: " << instr.opcode << endl;
            filaInstrucoes.pop();
            return;
        }
        for (auto& estacao : *estacoes) {
            if (!estacao.ocupado) {
                // Emissão da instrução para a estação de reserva
                estacao.ocupado = true;
                estacao.op = instr.opcode;
                estacao.destino = instr.rd;
                estacao.ciclosRestantes = ciclosExecucao;
                estacao.cicloEmissao = ciclo;

                // Verifica disponibilidade dos operandos
                if (registradores[instr.rs].Qi == "") {
                    estacao.Vj = to_string(registradores[instr.rs].valor);
                    estacao.Qj = "";
                } else {
                    estacao.Qj = registradores[instr.rs].Qi;
                    estacao.Vj = "";
                }
                if (registradores[instr.rt].Qi == "") {
                    estacao.Vk = to_string(registradores[instr.rt].valor);
                    estacao.Qk = "";
                } else {
                    estacao.Qk = registradores[instr.rt].Qi;
                    estacao.Vk = "";
                }
                // Atualiza o registrador de destino para apontar para esta estação
                registradores[instr.rd].Qi = estacao.nome;
                filaInstrucoes.pop();
                cout << setw(20) << "Emitido: " << estacao.op << " " << estacao.destino
                     << ", " << instr.rs << ", " << instr.rt << " para " << estacao.nome << endl;
                break;
            }
        }
    }

    void executarInstrucoes() {
        // Percorre todas as estações para verificar se podem iniciar a execução
        for (auto& estacao : estacoesAdd) {
            if (estacao.ocupado && estacao.cicloExecucaoInicio == 0 && estacao.Qj == "" && estacao.Qk == "") {
                estacao.cicloExecucaoInicio = ciclo;
                estacoesAtivas.push_back(&estacao);
                cout << setw(20) << "Inicio Execucao: " << estacao.nome << endl;
            }
        }
        for (auto& estacao : estacoesMul) {
            if (estacao.ocupado && estacao.cicloExecucaoInicio == 0 && estacao.Qj == "" && estacao.Qk == "") {
                estacao.cicloExecucaoInicio = ciclo;
                estacoesAtivas.push_back(&estacao);
                cout << setw(20) << "Inicio Execucao: " << estacao.nome << endl;
            }
        }
        // Decrementa o contador de ciclos das estações em execução
        for (auto it = estacoesAtivas.begin(); it != estacoesAtivas.end();) {
            EstacaoDeReserva* estacao = *it;
            if (estacao->ciclosRestantes > 0) {
                estacao->ciclosRestantes--;
                if (estacao->ciclosRestantes == 0) {
                    estacao->cicloExecucaoFim = ciclo;
                    cout << setw(20) << "Execucao Completa: " << estacao->nome << endl;
                    it = estacoesAtivas.erase(it); // Remove da lista de estações ativas
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }
    }

    void escreverResultado() {
        cdb.clear();
        int writesThisCycle = 0;
        // Percorre as estações de reserva para verificar se podem escrever no CDB
        for (auto& estacao : estacoesAdd) {
            if (writesThisCycle >= limiteCDB) break;
            if (estacao.ocupado && estacao.ciclosRestantes == 0 && estacao.cicloWriteBack == 0) {
                // Realiza o write-back
                int resultado = 0;
                int Vj = stoi(estacao.Vj);
                int Vk = stoi(estacao.Vk);
                if (estacao.op == "ADD") {
                    resultado = Vj + Vk;
                } else if (estacao.op == "SUB") {
                    resultado = Vj - Vk;
                }
                cdb.push_back(estacao.nome);
                atualizarRegistradores(estacao.destino, resultado, estacao.nome);
                estacao.cicloWriteBack = ciclo;
                estacao.ocupado = false; // Libera a estação
                estacao = {"Add" + estacao.nome.substr(3), false, "", "", "", "", "", 0, "", 0, 0, 0, 0};
                writesThisCycle++;
                cout << setw(20) << "Write-Back: " << estacao.nome << endl;
            }
        }
        for (auto& estacao : estacoesMul) {
            if (writesThisCycle >= limiteCDB) break;
            if (estacao.ocupado && estacao.ciclosRestantes == 0 && estacao.cicloWriteBack == 0) {
                // Realiza o write-back
                int resultado = 0;
                int Vj = stoi(estacao.Vj);
                int Vk = stoi(estacao.Vk);
                if (estacao.op == "MUL") {
                    resultado = Vj * Vk;
                } else if (estacao.op == "DIV") {
                    resultado = Vj / Vk;
                }
                cdb.push_back(estacao.nome);
                atualizarRegistradores(estacao.destino, resultado, estacao.nome);
                estacao.cicloWriteBack = ciclo;
                estacao.ocupado = false; // Libera a estação
                estacao = {"Mul" + estacao.nome.substr(3), false, "", "", "", "", "", 0, "", 0, 0, 0, 0};
                writesThisCycle++;
                cout << setw(20) << "Write-Back: " << estacao.nome << endl;
            }
        }
    }

    void atualizarRegistradores(string reg, int valor, string tag) {
        // Atualiza o registrador de destino se ainda estiver esperando por este valor
        if (registradores[reg].Qi == tag) {
            registradores[reg].valor = valor;
            registradores[reg].Qi = "";
        }
        // Atualiza as estações de reserva que estão esperando por este valor
        for (auto& estacao : estacoesAdd) {
            if (estacao.Qj == tag) {
                estacao.Vj = to_string(valor);
                estacao.Qj = "";
            }
            if (estacao.Qk == tag) {
                estacao.Vk = to_string(valor);
                estacao.Qk = "";
            }
        }
        for (auto& estacao : estacoesMul) {
            if (estacao.Qj == tag) {
                estacao.Vj = to_string(valor);
                estacao.Qj = "";
            }
            if (estacao.Qk == tag) {
                estacao.Vk = to_string(valor);
                estacao.Qk = "";
            }
        }
    }

    bool estaOcupado() {
        // Verifica se alguma estação de reserva está ocupada
        for (auto& estacao : estacoesAdd) {
            if (estacao.ocupado) return true;
        }
        for (auto& estacao : estacoesMul) {
            if (estacao.ocupado) return true;
        }
        return false;
    }

    void imprimirStatus() {
        // Imprime o status das estações de reserva e registradores
        cout << "Estacoes de Reserva:\n";
        for (auto& estacao : estacoesAdd) {
            cout << estacao.nome << ": " << (estacao.ocupado ? "Ocupado" : "Livre")
                 << ", Op: " << estacao.op
                 << ", Vj: " << estacao.Vj << ", Vk: " << estacao.Vk
                 << ", Qj: " << estacao.Qj << ", Qk: " << estacao.Qk
                 << ", Destino: " << estacao.destino
                 << ", Ciclos Restantes: " << estacao.ciclosRestantes << endl;
        }
        for (auto& estacao : estacoesMul) {
            cout << estacao.nome << ": " << (estacao.ocupado ? "Ocupado" : "Livre")
                 << ", Op: " << estacao.op
                 << ", Vj: " << estacao.Vj << ", Vk: " << estacao.Vk
                 << ", Qj: " << estacao.Qj << ", Qk: " << estacao.Qk
                 << ", Destino: " << estacao.destino
                 << ", Ciclos Restantes: " << estacao.ciclosRestantes << endl;
        }
        cout << "Registradores:\n";
        for (auto& reg : registradores) {
            cout << reg.first << ": " << reg.second.valor
                 << ", Qi: " << reg.second.Qi << endl;
        }
    }
};

queue<Instrucao> lerInstrucoes(string nomeArquivo) {
    ifstream infile(nomeArquivo);
    string linha;
    queue<Instrucao> filaInstr;
    while (getline(infile, linha)) {
        if (linha.empty()) continue;
        Instrucao instr;
        size_t pos = linha.find(' ');
        instr.opcode = linha.substr(0, pos);
        linha = linha.substr(pos + 1);
        pos = linha.find(',');
        instr.rd = linha.substr(0, pos);
        linha = linha.substr(pos + 2);
        pos = linha.find(',');
        instr.rs = linha.substr(0, pos);
        instr.rt = linha.substr(pos + 2);
        filaInstr.push(instr);
    }
    return filaInstr;
}

int main() {
    // Especifique o caminho do arquivo com as instruções
    std::string caminhoArquivo = "C:\\Users\\lucas\\OneDrive\\Desktop\\Lucas Moreira\\PUC\\QuintoPeriodo\\AC-III\\trab2\\in.txt"; // Ajuste o caminho conforme necessário

    // Ler instruções do arquivo especificado
    std::queue<Instrucao> filaInstr = lerInstrucoes(caminhoArquivo);

    // Inicializar e executar o simulador
    SimuladorTomasulo simulador(filaInstr);
    simulador.executar();
    return 0;
}
