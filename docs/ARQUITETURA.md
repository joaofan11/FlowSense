# Arquitetura técnica — FlowSense

## Visão geral
O FlowSense é um sistema embarcado de monitoramento de entradas e saídas com ESP32 DevKit V4, dois sensores infravermelhos e dashboard web local.

## Hardware
- ESP32 DevKit V4 / ESP32-WROOM-32.
- Sensores E18-D80NK NPN em sequência na porta, nos GPIOs 32 (A) e 33 (B).
- Saída NPN e alimentação do sensor devem ser compatibilizadas com os níveis de 3,3 V do ESP32.
- Powerbank; autonomia em torno de 12 h é estimativa, ainda não validada por ensaio.

## Lógica de contagem
- A seguido de B: entrada.
- B seguido de A: saída.
- Após uma passagem, ambos os sensores devem liberar a passagem antes de um novo evento.
- Valores da implementação de referência: antirruído 80 ms, liberação 300 ms, timeout entre sensores 1800 ms.
- A ocupação é estimativa baseada em ocupação inicial + entradas - saídas.

## Firmware
Linguagem: C++ na Arduino IDE com bibliotecas Arduino, WiFi, WebServer, DNSServer, LittleFS e Preferences. O ESP32 atua como ponto de acesso Wi-Fi, hospeda a interface e expõe APIs HTTP. A tela web é compilada como arquivo C++ header em PROGMEM.

## Interface
HTML5, CSS3 e JavaScript. Indicadores: entradas, saídas, passagens, ocupação, capacidade, alertas, movimentação por hora, histórico recente, exportação CSV e status.

## Persistência
- LittleFS: log de eventos CSV.
- Preferences/NVS: parâmetros e estado da sessão.
- Registro temporal depende da sincronização de relógio pelo navegador; sem isso, eventos podem não receber timestamp válido.

## Endpoints de referência
- `GET /api/status`
- `GET /api/hourly`
- `GET /api/export`
- `POST /api/config`
- `POST /api/session`
- `POST /api/time`

As rotas exatas e parâmetros devem ser conferidos na versão de firmware efetivamente instalada antes de integrações externas.
