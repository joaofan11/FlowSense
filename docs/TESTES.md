# Plano de testes do FlowSense

## Testes funcionais
| ID | Cenário | Resultado esperado |
|---|---|---|
| T01 | Uma pessoa entra (A→B) | Entradas +1 |
| T02 | Uma pessoa sai (B→A) | Saídas +1 |
| T03 | Pessoa passa sem completar sequência | Não conta evento completo |
| T04 | Duas pessoas com espaço | Conta duas passagens |
| T05 | Duas pessoas muito próximas | Medir taxa de erro |
| T06 | Passagens simultâneas opostas | Registrar limitação e comportamento |
| T07 | Exceder capacidade configurada | Alerta vermelho |
| T08 | Reinicializar ESP32 | Configurações e histórico persistem |
| T09 | Baixar CSV por dia/mês/ano | Exportação corresponde ao intervalo |
| T10 | Abrir painel sem internet | Interface e monitoramento local operam |

## Protocolo de precisão
Para cada situação, anote número de passagens reais, entradas registradas, saídas registradas, falsos positivos e eventos ausentes. Repita com diferentes velocidades e distâncias entre pessoas. Só divulgue percentuais após obter a amostra e os resultados.

## Autonomia
Cronometre o tempo de operação contínua da powerbank com sensores e Wi-Fi ligados. A autonomia de ~12 horas é estimativa até que exista teste mensurado.
