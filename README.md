# SENTINELA-Fall-Detection
"Sistema vestível de baixo custo para detecção de quedas em idosos, utilizando ESP32-C3, sensores inerciais e localização indoor via BLE Beacon, com alertas contextualizados via WhatsApp."
SENTINELA: Sistema Vestível para Detecção de Quedas de Idosos
O SENTINELA é um protótipo de sistema vestível de baixo custo desenvolvido para detetar quedas em idosos, estimar a localização interna por proximidade e enviar alertas automáticos contextualizados.

📝 Resumo do Projeto
O projeto utiliza sensoriamento inercial e comunicação sem fios para reduzir o tempo de resposta em caso de acidentes domésticos. Ao contrário de alarmes genéricos, o SENTINELA informa o setor provável (ex: quarto, casa de banho) onde o evento ocorreu, facilitando o socorro.

🛠️ Arquitetura de Hardware
A implementação consolidada utiliza os seguintes componentes principais:
Microcontrolador: ESP32-C3 SuperMini.

Sensor Inercial: MPU6050 (Acelerómetro e Giroscópio) alimentado a 3,3 V.

Gestão de Energia: Bateria Li-Po de 3,7 V, módulo de carga TP4056 e conversor boost MT3608 (ajustado para 5,0 V).

Interfaces Locais: Botão de pânico em GPIO7 e Buzzer ativo em GPIO1 (via transistor BC547).

💻 Arquitetura de Software
O sistema está organizado em quatro camadas modulares:

Borda (Edge): Processamento local no wearable (leitura de sensores e detecção de quedas via Máquina de Estados Finitos).

Comunicação: Transporte de dados via Wi-Fi e localização indoor por proximidade via BLE (Bluetooth Low Energy).

Serviço: Notificação contextualizada enviada ao cuidador através da API Twilio para o WhatsApp.

Evolução Computacional: Prevê a futura integração com bases de dados para persistência e painéis históricos.

🚀 Funcionalidades Principais

Detecção de Quedas: Algoritmo baseado em padrões inerciais (pico de aceleração e mudança de orientação).

Localização Indoor: Classificação por setor baseada no beacon Bluetooth dominante, utilizando filtragem por média móvel e histerese.

Alertas Contextualizados: Envio de mensagens automáticas que incluem o tipo de evento, horário e local estimado.

Minimização de Dados: Fluxo de informação alinhado com a LGPD, restringindo-se ao essencial para o socorro assistivo.

📈 Evoluções Futuras
Embora validado em bancada, o projeto prevê melhorias como:

Implementação de backend em Node.js e MongoDB para histórico de eventos.

Miniaturização do circuito em PCB dedicada.

Refinamento do algoritmo para redução de falsos positivos.

🎓 Autor
Augusto Cesar Prusch Machado - Graduando em Engenharia da Computação pela UniFECAF.
