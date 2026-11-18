# FlowSense

Sistema de monitoramento de fluxo e ocupação de pessoas usando **ESP32**, sensores infravermelhos e dashboard web local.

## Visão geral
O FlowSense registra passagens em uma porta e disponibiliza métricas agregadas de entradas, saídas e ocupação estimada. O protótipo acadêmico integra dois sensores E18-D80NK, ESP32 DevKit V4, histórico persistente e dashboard desenvolvido em HTML, CSS e JavaScript.

## Funcionamento
- Sensor A: GPIO 32; sensor B: GPIO 33.
- A → B: entrada; B → A: saída.
- ESP32 programado em C++ na Arduino IDE.
- Wi-Fi local e portal cativo, sem internet obrigatória.
- Histórico em memória Flash e relatórios CSV.
- Nome do local e capacidade configuráveis; indicador de excesso de ocupação.

## Estado
A equipe confirmou o protótipo funcional. Precisão em passagens simultâneas e autonomia precisam de testes documentados.

## Segurança
Equipamento experimental, sem certificação de controle oficial de lotação. Confirmar compatibilidade elétrica de 3,3 V nos GPIOs. Proteger endpoints administrativos antes de uso em produção.
