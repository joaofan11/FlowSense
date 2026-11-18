# Instalação e configuração

## Arduino IDE
1. Instale a plataforma **esp32 by Espressif Systems**.
2. Selecione **ESP32 Dev Module**.
3. Confirme Flash 4 MB e escolha esquema de partição que tenha filesystem (por exemplo, `Default 4MB with spiffs`; LittleFS utiliza a partição disponível).
4. Coloque `FlowSense_v3_Corrigido.ino` e `FlowSense_Dashboard.h` na mesma pasta.
5. Ative o modo de dois E18-D80NK no firmware, somente após confirmar ligações e lógica de nível.
6. Compile e envie ao ESP32; abra o Monitor Serial em 115200.
7. Conecte-se à rede local do FlowSense e abra `http://192.168.4.1`.

## Sensores
- A: GPIO 32; B: GPIO 33.
- Alimentação conforme especificação do seu módulo E18-D80NK, normalmente 5 V.
- A saída digital **nunca pode aplicar diretamente 5 V ao GPIO**. Validar saída NPN, resistores pull-up para 3,3 V e GND compartilhado.

## Verificação
- Passar A → B e verificar entrada.
- Passar B → A e verificar saída.
- Conferir histórico e exportação CSV.
- Alterar nome e capacidade; verificar alerta quando ultrapassar limite.
- Reiniciar o ESP32 e verificar persistência.
- Testar duas pessoas próximas e passagens simultâneas, anotando discrepâncias.

## Atenção
A troca do esquema de partição ou formatação do filesystem pode apagar registros. Faça backup por CSV.
