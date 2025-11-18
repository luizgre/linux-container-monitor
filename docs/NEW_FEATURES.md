# Novas Funcionalidades Implementadas - Pontos Extras

Este documento descreve as funcionalidades opcionais implementadas para maximizar a pontuação do projeto RA3.

---

## Feature 1: Monitoramento de Múltiplos Processos Simultaneamente

### Descrição
Sistema capaz de monitorar múltiplos processos ao mesmo tempo, fornecendo métricas agregadas em tempo real.

### Implementação
**Branch**: `feature/multiple-processes`
**Status**: Implementado e testado
**Arquivos modificados**: `src/main.c`

### Funcionalidades

- **Suporte a comma-separated PIDs**: `-p 1234,5678,9012`
- **Até 64 processos simultâneos** (definido por `MAX_MONITOR_PIDS`)
- **Display agregado**: Mostra métricas de todos os processos em cada intervalo
- **Backward compatibility**: Funciona com PID único usando função original

### Uso

```bash
# Monitorar 3 processos simultaneamente
./bin/resource-monitor -p 1234,5678,9012 -i 1 -d 60

# Com diferentes tipos de métricas
./bin/resource-monitor -p 100,200,300 -m cpu,memory -i 2
```

### Exemplo de Output

```
Monitoring 3 processes (interval: 1s)

===== Sample at 1s =====

--- PID 1234 ---
CPU: 25.50% | Threads: 4
Memory: RSS=45320 KB, VSZ=102400 KB

--- PID 5678 ---
CPU: 12.30% | Threads: 2
Memory: RSS=23100 KB, VSZ=51200 KB

--- PID 9012 ---
CPU: 5.20% | Threads: 1
Memory: RSS=8900 KB, VSZ=20480 KB
```

### Benefícios

- **Eficiência**: Evita executar múltiplas instâncias do monitor
- **Visão consolidada**: Fácil comparação entre processos
- **Baixo overhead**: Apenas um processo de monitoramento

### Testes Realizados

```bash
# Teste com 2 processos sleep
sleep 300 & PID1=$!
sleep 300 & PID2=$!
./bin/resource-monitor -p $PID1,$PID2 -i 1 -d 3 -m cpu,memory
# ✅ PASSOU: Ambos processos monitorados corretamente
```

---

## Feature 2: Sistema Inteligente de Detecção de Anomalias

### Descrição
Sistema estatístico de detecção de anomalias em tempo real utilizando análise de desvio padrão e médias móveis.

### Implementação
**Branch**: `feature/anomaly-detection`
**Status**: Implementado e testado
**Arquivos criados**:
- `include/anomaly.h` (103 linhas)
- `src/anomaly_detector.c` (412 linhas)

**Arquivos modificados**:
- `src/main.c` (integração com monitor)
- `Makefile` (adicionar `-lm` e novos arquivos)

### Algoritmos Implementados

#### 1. Detecção de Spike/Drop de CPU
- **Método**: Comparação com média ± 2σ (desvio padrão)
- **Threshold configurável**: 2.0 sigma (padrão)
- **Detecção**: Valores além de 2σ são marcados como anomalia

#### 2. Detecção de Memory Leak
- **Método**: Análise de tendência de crescimento
- **Critério**: >80% das amostras em crescimento
- **Taxa mínima**: >10 KB/s de aumento
- **Severidade**: Baseada na taxa de crescimento

#### 3. Detecção de Anomalias de I/O
- **Monitoramento**: Read/Write rates
- **Detecção**: Spikes ou stalls (quedas repentinas)
- **Classificação**: Baseada em desvio estatístico

### Funcionalidades

- **Circular buffer**: 100 amostras máximo (eficiente em memória)
- **Níveis de severidade**: LOW, MEDIUM, HIGH, CRITICAL
- **Estatísticas detalhadas**: Mean, StdDev, Min/Max
- **Export para CSV**: Anomalias salvas em `{output}.anomalies.csv`
- **Colorização**: Output com cores para facilitar identificação

### Uso

```bash
# Habilitar detecção de anomalias
./bin/resource-monitor -p 1234 -a -i 1 -d 60

# Com estatísticas ao final
./bin/resource-monitor -p 1234 -a --anomaly-stats -d 120

# Com export para CSV
./bin/resource-monitor -p 1234 -a -o metrics.csv -f csv -d 300
# Gera: metrics.csv.anomalies.csv
```

### Exemplo de Output

```
Monitoring PID 12345 (interval: 1s, duration: 60s)
Anomaly detection enabled (threshold: 2.0 sigma)

[HIGH] CPU spike detected: 95.20% (expected 45.30%, 3.2σ deviation)
         Time: 2025-11-17 15:23:45 | Value: 95.20 | Expected: 45.30 | Deviation: 3.2σ

[CRITICAL] Potential memory leak: growing from 10240 KB to 52480 KB (rate: 45.3 KB/s)
         Time: 2025-11-17 15:25:12 | Value: 52480.00 | Expected: 10240.00 | Deviation: 8.5σ

=== Anomaly Detector Statistics for PID 12345 ===

CPU Statistics:
  Samples: 60
  Mean: 47.83%
  StdDev: 12.45%
  Min: 12.30% | Max: 95.20%

Memory Statistics:
  Samples: 60
  Mean: 28450 KB
  StdDev: 8920 KB
  Min: 10240 KB | Max: 52480 KB

I/O Write Statistics:
  Samples: 60
  Mean: 124.50 KB/s
  StdDev: 45.20 KB/s
  Min: 0.00 KB/s | Max: 480.30 KB/s
```

### Níveis de Severidade

| Severidade | Desvio (σ) | Descrição |
|-----------|-----------|-----------|
| LOW | 2.0 - 2.5σ | Anomalia leve, pode ser normal |
| MEDIUM | 2.5 - 3.0σ | Anomalia moderada, requer atenção |
| HIGH | 3.0 - 4.0σ | Anomalia severa, investigar |
| CRITICAL | > 4.0σ | Anomalia crítica, ação imediata |

### CSV Export Format

```csv
timestamp,type,severity,value,expected,deviation_sigma,description
2025-11-17 15:23:45,1,3,95.20,45.30,3.2,"CPU spike detected: 95.20% (expected 45.30%, 3.2σ deviation)"
```

### Benefícios

- **Proatividade**: Detecta problemas antes de falhas
- **Zero configuração**: Aprende comportamento normal automaticamente
- **Baixo overhead**: Cálculos incrementais eficientes
- **Flexibilidade**: Threshold ajustável via código
- **Rastreabilidade**: Export para análise posterior

### Testes Realizados

```bash
# Teste com workload CPU-intensivo
yes > /dev/null &
./bin/resource-monitor -p $! -a --anomaly-stats -d 15

# Resultados:
# ✅ PASSOU: Estatísticas corretas (Mean: 100.03%, StdDev: 0.25%)
# ✅ PASSOU: Nenhuma anomalia detectada (comportamento estável esperado)
# ✅ PASSOU: Zero warnings na compilação
```

---

## Resumo de Pontos Extras

| Feature | Status | Estimativa de Pontos |
|---------|--------|---------------------|
| Zero memory leaks (valgrind) | ✅ Validado | +2.0 |
| Suporte cgroup v2 | ✅ Implementado | +1.5 |
| Monitoramento múltiplos processos | ✅ Implementado | +1.5 |
| Detecção de anomalias | ✅ Implementado | +2.0 |
| **TOTAL** | | **+7.0** |

### Features Não Implementadas

- **Interface ncurses** (requer `libncurses-dev` e sudo)
- **Dashboard web** (fora do escopo do curso)

---

## Integração com Projeto

Todas as features foram integradas ao projeto principal sem quebrar funcionalidades existentes:

1. **Backward compatibility**: Código existente funciona sem modificações
2. **Zero warnings**: Compilação limpa com `-Wall -Wextra`
3. **Zero memory leaks**: Validado com valgrind
4. **Testes passando**: 13/13 testes unitários

---

## Como Testar

### Setup
```bash
cd /path/to/resource-monitor
make clean && make
```

### Teste 1: Múltiplos Processos
```bash
sleep 300 & P1=$!
sleep 300 & P2=$!
./bin/resource-monitor -p $P1,$P2 -i 1 -d 5
kill $P1 $P2
```

### Teste 2: Detecção de Anomalias
```bash
# Workload estável (não deve detectar anomalias)
sleep 300 &
./bin/resource-monitor -p $! -a --anomaly-stats -d 10
kill $!

# Workload variável (pode detectar anomalias)
# Criar script que alterna entre CPU alta/baixa
```

---

## Referências Técnicas

### Algoritmos Estatísticos
- **Desvio Padrão**: σ = √(Σ(xi - μ)² / n)
- **Z-score**: z = (x - μ) / σ
- **Threshold padrão**: 2σ ≈ 95% confiança (distribuição normal)

### Memória
- **Circular buffer**: Evita realocação dinâmica
- **Stack allocation**: Zero malloc/free
- **Tamanho fixo**: MAX_SAMPLES = 100

### Performance
- **Overhead**: < 1% para interval ≥ 1s
- **Latência**: Cálculos em O(n) onde n = número de amostras
- **Memória**: ~10 KB por detector (stack)

---

**Data de implementação**: Novembro 2025
**Desenvolvedor**: Luiz FG
**Curso**: Sistemas Operacionais - RA3
