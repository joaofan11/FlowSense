# Limitações e próximos passos

- Protótipo acadêmico funcional, mas precisão quantitativa ainda não certificada.
- Pessoas lado a lado, muito próximas ou em sentidos opostos podem gerar contagem ambígua.
- Ocupação é uma estimativa e pode acumular erro ao longo do tempo.
- Alimentação por powerbank não impede perdas de eventos quando descarregada.
- Flash e LittleFS têm capacidade e ciclo de vida limitados; exportar dados regularmente.
- Sem relógio independente, o ESP32 depende da sincronização por navegador para registrar horários confiáveis.
- O painel local deve receber autenticação e controles de acesso antes de uma implantação comercial.
- Não usar como equipamento certificado de evacuação, controle de lotação ou segurança obrigatória.

## Evolução planejada
1. Testes de precisão e autonomia com resultados documentados.
2. Suporte a cartão microSD e políticas de retenção.
3. Autenticação e backup seguro.
4. Sincronização confiável de horário e dashboards multiambiente.
5. Integração opcional com nuvem, preservando funcionamento offline.
