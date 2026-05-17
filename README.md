# SENTINELA: Sistema Vestível para Detecção de Quedas de Idosos

O **SENTINELA** é um sistema vestível (*wearable*) de baixo custo desenvolvido para a detecção automática de quedas em idosos, integrando estimativa de localização indoor e alertas contextualizados via WhatsApp[cite: 4, 11, 160, 167].

## 📝 Resumo do Projeto
O sistema utiliza processamento embarcado para detetar eventos críticos e fornecer uma resposta assistiva ágil[cite: 26, 40, 180, 193]. [cite_start]A localização é estimada por proximidade com **BLE Beacons**, permitindo identificar o setor (ex: sala, quarto, casa de banho) onde ocorreu o evento, oferecendo valor operacional superior a alarmes genéricos[cite: 4, 25, 65, 179, 216].

## 🛠️ Arquitetura de Hardware
A implementação consolidada utiliza os seguintes componentes principais[cite: 6, 13, 162, 169]:
**Microcontrolador:** ESP32-C3 SuperMini[cite: 6, 116, 162, 260].
**Sensor Inercial:** MPU6050 para leitura de aceleração e orientação através de algoritmos de baixa complexidade computacional[cite: 6, 54, 83, 162, 205, 232].
**Interface Local:** Botão de pânico em GPIO7 para acionamento manual e Buzzer ativo de 3V em GPIO1 para feedback sonoro[cite: 6, 85, 86, 162, 234, 235].
**Gestão de Energia:** Bateria Li-Po de 3,7 V, módulo de carga TP4056 e conversor boost MT3608 ajustado para 5,0 V para estabilizar a alimentação[cite: 6, 13, 87, 162, 169, 236].

## 📡 Comunicação e Camada de Serviço
O projeto utiliza uma arquitetura em camadas para garantir modularidade e manutenção[cite: 45, 198]:
**Wi-Fi:** Responsável pelo envio de notificações via API Twilio para o WhatsApp do cuidador[cite: 26, 72, 92, 180, 221, 239].
**Bluetooth Low Energy (BLE):** Utilizado para a varredura de beacons e classificação de setor por proximidade[cite: 65, 92, 216, 239].

## 🔒 Segurança e Privacidade
**AVISO IMPORTANTE:** Seguindo as melhores práticas de segurança e visando a proteção de dados pessoais, todas as informações sensíveis foram removidas ou substituídas por *placeholders* no código-fonte disponível:
* **SSID/Password:** Credenciais de rede Wi-Fi removidas.
* **Twilio Auth:** Chaves de autenticação (*Account SID* e *Auth Token*) da API de comunicação removidas.
* **Telefones:** Números de contacto de emergência ocultados.

## 📊 Resultados e Validação
Os testes de bancada confirmaram a integridade elétrica e a viabilidade funcional do fluxo ponta a ponta (evento -> localização -> alerta)[cite: 7, 106, 163, 250].
**Estabilidade:** A nova topologia com MT3608 estabilizou o protótipo durante operações simultâneas de Wi-Fi e BLE[cite: 90, 102, 237, 250].
**Ambiente de Análise:** Os scripts de visualização de dados e métricas de sinal (RSSI) utilizados na defesa podem ser consultados via **Google Colab** através do link encurtado nos slides da apresentação.

## 🎓 Autor
**Augusto Cesar Prusch Machado** - Graduando em Engenharia da Computação pela UniFECAF[cite: 3, 17, 32, 158].
