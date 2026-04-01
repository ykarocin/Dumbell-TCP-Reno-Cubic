import matplotlib.pyplot as plt
import os

def ler_dados(nome_arquivo):
    """Lê as duas colunas (Tempo e Valor) de um arquivo .dat"""
    tempos = []
    valores = []
    if os.path.exists(nome_arquivo):
        with open(nome_arquivo, 'r') as f:
            for linha in f:
                partes = linha.strip().split()
                if len(partes) >= 2:
                    tempos.append(float(partes[0]))
                    valores.append(float(partes[1]))
        print(f"✅ {nome_arquivo} carregado com sucesso.")
    else:
        print(f"⚠️ Aviso: Arquivo {nome_arquivo} não encontrado.")
    return tempos, valores

def plotar_cwnd():
    """Gera o gráfico comparativo da Janela de Congestionamento"""
    t_reno, cwnd_reno = ler_dados('cwnd_reno.dat')
    t_cubic, cwnd_cubic = ler_dados('cwnd_cubic.dat')

    plt.figure(figsize=(10, 5))
    
    if t_reno:
        plt.plot(t_reno, cwnd_reno, label='TCP Reno', color='blue', linewidth=1.5)
    if t_cubic:
        plt.plot(t_cubic, cwnd_cubic, label='TCP Cubic', color='red', linewidth=1.5, alpha=0.8)

    plt.title('Evolução da Janela de Congestionamento (S1)')
    plt.xlabel('Tempo da Simulação (segundos)')
    plt.ylabel('Cwnd (Bytes)')
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.legend()
    plt.tight_layout()
    
    nome_saida = 'grafico_cwnd.png'
    plt.savefig(nome_saida, dpi=300)
    print(f"📊 Gráfico salvo como {nome_saida}\n")

def plotar_throughput():
    """Gera o gráfico comparativo da Taxa de Transferência no tempo"""
    t_reno, thr_reno = ler_dados('throughput_reno.dat')
    t_cubic, thr_cubic = ler_dados('throughput_cubic.dat')

    plt.figure(figsize=(10, 5))
    
    if t_reno:
        plt.plot(t_reno, thr_reno, label='TCP Reno', color='blue', linewidth=1.5)
    if t_cubic:
        plt.plot(t_cubic, thr_cubic, label='TCP Cubic', color='red', linewidth=1.5, alpha=0.8)

    plt.title('Taxa de Transferência do TCP no Tempo (S1 -> D1)')
    plt.xlabel('Tempo da Simulação (segundos)')
    plt.ylabel('Throughput (Mbps)')
    
    # Adicionando uma linha de referência para o limite que a aplicação tentou enviar (5 Mbps)
    plt.axhline(y=5.0, color='green', linestyle=':', label='Limite da Aplicação (5 Mbps)')

    plt.grid(True, linestyle='--', alpha=0.7)
    plt.legend()
    plt.tight_layout()
    
    nome_saida = 'grafico_throughput.png'
    plt.savefig(nome_saida, dpi=300)
    print(f"📊 Gráfico salvo como {nome_saida}\n")

if __name__ == '__main__':
    print("Iniciando geração dos gráficos...\n")
    print("--------------------------------------------------")
    plotar_cwnd()
    plotar_throughput()
    print("--------------------------------------------------")
    print("Pronto! Agora é só colar as imagens nos seus slides.")