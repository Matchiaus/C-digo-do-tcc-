// ==========================================
// ROBÔ 4X4 - DASHBOARD WEB INTEGRADO (V2 - CORRIGIDO)
// CONTROLE DE PAUSA SUAVE + REATIVAÇÃO DE LÂMINA
// VERSÃO PARA 4x DRIVERS TMC2209 (STANDALONE)
// ==========================================

#include <math.h>
#include <stdlib.h>

#define rele 23
#define pinoEcho 22
#define pinoTrig 24

const float terrenoMinH = 0.5;
const float terrenoMinB = 0.2;
const float terrenoMaximo = 100.0;
const uint8_t tamanhoMaximoComando = 48;

// --- LADO ESQUERDO (CNC Shield) ---
const int pinoPassoY = 3;
const int pinoDirecaoY = 6;
const int pinoPassoA = 12;
const int pinoDirecaoA = 13;

// --- LADO DIREITO (CNC Shield) ---
const int pinoPassoX = 2;
const int pinoDirecaoX = 5;
const int pinoPassoZ = 4;
const int pinoDirecaoZ = 7;

const int pinoEnable = 8;

// --- CONFIGURAÇÕES DE FORÇA E MEDIDAS ---
// Para TMC2209 Standalone, velocidadeCruzeiro define a lentidão do pulso (maior valor = maior torque)
const int velocidadeCruzeiro = 12000;  
const int passosPorMetro = 1000;
const int passosPara90Graus = 180;
const float larguraRobo = 0.2;  // 20 cm

// Variáveis de Área via Bluetooth
float valorH_metros = 0.0;
float valorB_metros = 0.0;

// Variáveis de Missão
long passosGlobaisTotais = 0;
long passosGlobaisCompletos = 0;
int ultimaPorcentagem = -1;
String estadoRobo = "IDLE";  // IDLE, RUNNING, PAUSED, CANCELLED

// Leitura Bluetooth Limpa
String bufferBluetooth = "";
bool descartandoComando = false;
bool pausarAssimQueIniciar = false;

// Controle de sensores
unsigned long ultimoTempoUltrassonico = 0;
const unsigned long intervaloUltrassonico = 100;
bool pausaPorObstaculo = false;

void setup() {
  Serial.begin(9600);

  pinMode(pinoTrig, OUTPUT);
  pinMode(pinoEcho, INPUT);

  pinMode(rele, OUTPUT);
  digitalWrite(rele, LOW);  // Lâmina Desligada por padrão

  pinMode(pinoPassoY, OUTPUT);
  pinMode(pinoDirecaoY, OUTPUT);
  pinMode(pinoPassoA, OUTPUT);
  pinMode(pinoDirecaoA, OUTPUT);
  pinMode(pinoPassoX, OUTPUT);
  pinMode(pinoDirecaoX, OUTPUT);
  pinMode(pinoPassoZ, OUTPUT);
  pinMode(pinoDirecaoZ, OUTPUT);

  pinMode(pinoEnable, OUTPUT);
  digitalWrite(pinoEnable, LOW);  // Trava motores para ter força

  Serial.println("--- ROBO PRONTO PARA O DASHBOARD WEB (TMC2209) ---");
}

void loop() {
  verificarBluetoothGeral();
}

// ==========================================
// LEITURA NÃO-BLOQUEANTE DO BLUETOOTH
// ==========================================
void verificarBluetoothGeral() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (!descartandoComando) {
        bufferBluetooth.trim();
        bufferBluetooth.toUpperCase();
        
        String comandoLimpo = "";
        for (unsigned int i = 0; i < bufferBluetooth.length(); i++) {
          char recebido = bufferBluetooth.charAt(i);
          if (recebido >= 32 && recebido <= 126) comandoLimpo += recebido;
        }
        comandoLimpo.trim();
        if (comandoLimpo.length() > 0) processarComando(comandoLimpo);
      } else {
        Serial.println("[ERRO] Comando muito longo; linha ignorada.");
      }
      bufferBluetooth = "";
      descartandoComando = false;
    } else {
      if (!descartandoComando) {
        if (bufferBluetooth.length() < tamanhoMaximoComando) bufferBluetooth += c;
        else descartandoComando = true;
      }
    }
  }
}

// Executa a ordem enviada pelo Site
void processarComando(String comando) {
  if (comando.startsWith("STARTPAUSE")) {
    if (estadoRobo == "IDLE" && valorH_metros >= terrenoMinH && valorB_metros >= terrenoMinB) {
      pausarAssimQueIniciar = true;
      iniciarMissao();
    } else {
      Serial.println("[ERRO] STARTPAUSE requer robô parado e dimensões válidas.");
    }
  } else if (comando == "START") {
    if (estadoRobo == "RUNNING" || estadoRobo == "PAUSED") {
      Serial.println("[ERRO] Já existe uma missão ativa.");
    } else if (estadoRobo == "IDLE" && valorH_metros >= terrenoMinH && valorB_metros >= terrenoMinB) {
      iniciarMissao();
    } else if (estadoRobo != "IDLE") {
      Serial.println("[ERRO] Aguarde a missão atual parar antes de iniciar outra.");
    } else {
      Serial.println("[ERRO] Configure H e B válidos antes do START.");
    }
  } else if (comando.startsWith("PAUSE")) {
    if (estadoRobo != "RUNNING") {
      if (estadoRobo == "PAUSED") Serial.println("PAUSE");
      return;
    }
    estadoRobo = "PAUSED";
    digitalWrite(rele, LOW);
    Serial.println("PAUSE");
  } else if (comando == "RESUME") {
    if (estadoRobo == "PAUSED") {
      estadoRobo = "RUNNING";
      digitalWrite(rele, pausaPorObstaculo ? LOW : HIGH);
      Serial.println(pausaPorObstaculo ? "PAUSE" : "RESUME");
    } else if (estadoRobo == "RUNNING" && pausaPorObstaculo) {
      Serial.println("PAUSE");
    }
  } else if (comando == "CANCEL" || comando == "HOME") {
    if (estadoRobo != "RUNNING" && estadoRobo != "PAUSED") return;
    estadoRobo = "CANCELLED";
    digitalWrite(rele, LOW);
    pausaPorObstaculo = false;
  } else if (comando.startsWith("H") || comando.startsWith("B")) {
    if (estadoRobo != "IDLE") {
      Serial.println("[ERRO] Dimensões bloqueadas durante a missão.");
      return;
    }

    char eixo = comando.charAt(0);
    String textoValor = comando.substring(1);
    textoValor.trim();
    bool formatoValido = textoValor.length() > 0;
    bool encontrouDigito = false;
    bool encontrouPonto = false;
    
    for (unsigned int i = 0; i < textoValor.length(); i++) {
      char c = textoValor.charAt(i);
      if (c >= '0' && c <= '9') {
        encontrouDigito = true;
      } else if (c == '.' && !encontrouPonto) {
        encontrouPonto = true;
      } else {
        formatoValido = false;
        break;
      }
    }

    if (!formatoValido || !encontrouDigito) {
      Serial.println("[ERRO] Dimensão inválida. Use Hnúmero ou Bnúmero.");
      return;
    }
    float valor = textoValor.toFloat();

    float minimo = eixo == 'H' ? terrenoMinH : terrenoMinB;
    if (valor < minimo || valor > terrenoMaximo) {
      Serial.print("[ERRO] ");
      Serial.print(eixo);
      Serial.print(" deve estar entre ");
      Serial.print(minimo, 1);
      Serial.println(" e 100 m.");
      return;
    }

    if (eixo == 'H') valorH_metros = valor;
    else valorB_metros = valor;
    Serial.print("[OK] ");
    Serial.print(eixo);
    Serial.print('=');
    Serial.println(valor, 2);
  } else {
    Serial.print("[ERRO] Comando desconhecido: [");
    Serial.print(comando);
    Serial.println("]");
  }
}

// ==========================================
// LÓGICA PRINCIPAL DA MISSÃO
// ==========================================
void iniciarMissao() {
  estadoRobo = "RUNNING";
  Serial.println("START");
  if (pausarAssimQueIniciar) {
    pausarAssimQueIniciar = false;
    estadoRobo = "PAUSED";
    digitalWrite(rele, LOW);
    Serial.println("PAUSE");
  } else {
    digitalWrite(rele, HIGH);  // Liga motor da Lâmina
  }

  int numeroDeViagens = max(1, (int)ceil(valorB_metros / larguraRobo));

  long passosH = valorH_metros * passosPorMetro;
  long passosDeslocamentoLado = larguraRobo * passosPorMetro;

  long totalPassosRetos = numeroDeViagens * passosH;
  long totalPassosLaterais = (numeroDeViagens - 1) * passosDeslocamentoLado;
  long totalPassosCurvas = (numeroDeViagens - 1) * (2 * passosPara90Graus);

  passosGlobaisTotais = totalPassosRetos + totalPassosLaterais + totalPassosCurvas;
  passosGlobaisCompletos = 0;
  ultimaPorcentagem = -1;

  for (int v = 1; v <= numeroDeViagens; v++) {
    if (estadoRobo == "CANCELLED") break;

    // ANDAR PARA A FRENTE (TMC2209: Esq HIGH, Dir LOW)
    moverRobo(passosH, HIGH, LOW);

    if (v < numeroDeViagens && estadoRobo != "CANCELLED") {
      if (v % 2 != 0) {
        girarDireita();
        moverRobo(passosDeslocamentoLado, HIGH, LOW);
        girarDireita();
      } else {
        girarEsquerda();
        moverRobo(passosDeslocamentoLado, HIGH, LOW);
        girarEsquerda();
      }
    }
  }

  digitalWrite(rele, LOW);  // Desliga lâmina no fim

  if (estadoRobo == "CANCELLED") {
    Serial.println("CANCEL");
    estadoRobo = "IDLE";
  } else {
    estadoRobo = "IDLE";
    Serial.println("DONE");
  }
}

// ==========================================
// MOVIMENTAÇÃO DOS MOTORES (COM PAUSA SUAVE)
// ==========================================
void moverRobo(long passosTotais, bool dirEsq, bool dirDir) {
  // Configura a direção de cada lado
  digitalWrite(pinoDirecaoY, dirEsq);
  digitalWrite(pinoDirecaoA, dirEsq);
  digitalWrite(pinoDirecaoX, dirDir);
  digitalWrite(pinoDirecaoZ, dirDir);

  long passosExecutados = 0;
  bool estadoPinoPasso = LOW;
  unsigned long ultimoTempoPasso = micros();

  while (passosExecutados < passosTotais) {

    verificarBluetoothGeral();

    if (estadoRobo == "CANCELLED") {
      return;
    }

    bool estavaPausado = false;

    // Se o usuário clicou em PAUSE no site, ou encontrou obstáculo, prende aqui
    while (estadoRobo == "PAUSED" || pausaPorObstaculo) {
      estavaPausado = true;
      verificarObstaculoComMillis(millis());
      verificarBluetoothGeral();  

      if (estadoRobo == "CANCELLED") return;
    }

    if (estavaPausado) {
      ultimoTempoPasso = micros();
    }

    verificarObstaculoComMillis(millis());

    // Disparo dos passos 4x4
    if (micros() - ultimoTempoPasso >= velocidadeCruzeiro) {
      ultimoTempoPasso = micros();

      estadoPinoPasso = !estadoPinoPasso;
      digitalWrite(pinoPassoY, estadoPinoPasso);
      digitalWrite(pinoPassoA, estadoPinoPasso);
      digitalWrite(pinoPassoX, estadoPinoPasso);
      digitalWrite(pinoPassoZ, estadoPinoPasso);

      if (estadoPinoPasso == LOW) {
        passosExecutados++;
        passosGlobaisCompletos++;

        // Atualiza barra do site
        int porcentagemAtual = passosGlobaisTotais > 0
                                 ? (int)(100.0f * (float)passosGlobaisCompletos / (float)passosGlobaisTotais)
                                 : 100;
        porcentagemAtual = constrain(porcentagemAtual, 0, 100);
        
        if (porcentagemAtual != ultimaPorcentagem) {
          Serial.print("Progresso: ");
          Serial.print(porcentagemAtual);
          Serial.println("%");
          ultimaPorcentagem = porcentagemAtual;
        }
      }
    }
  }
}

// ==========================================
// SEGURANÇA ULTRASSÔNICA
// ==========================================
long lerDistanciaCM() {
  digitalWrite(pinoTrig, LOW);
  delayMicroseconds(2);
  digitalWrite(pinoTrig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pinoTrig, LOW);
  long duracao = pulseIn(pinoEcho, HIGH, 2000);
  if (duracao == 0) return 999;
  return (duracao * 0.034) / 2;
}

void verificarObstaculoComMillis(unsigned long tempoAtualMillis) {
  if (tempoAtualMillis - ultimoTempoUltrassonico >= intervaloUltrassonico) {
    ultimoTempoUltrassonico = tempoAtualMillis;

    long distancia = lerDistanciaCM();

    if (distancia <= 20 && distancia > 0) {
      if (!pausaPorObstaculo) {
        pausaPorObstaculo = true;
        digitalWrite(rele, LOW);
        Serial.println("PAUSE");
        Serial.print("[ALERTA] Obstaculo a ");
        Serial.print(distancia);
        Serial.println("cm!");
      }
    } else {
      if (pausaPorObstaculo) {
        pausaPorObstaculo = false;
        if (estadoRobo == "RUNNING") {
          digitalWrite(rele, HIGH);
          Serial.println("RESUME");
        }
        Serial.println("[OK] Pista livre.");
      }
    }
  }
}

// ==========================================
// FUNÇÕES DE CURVA
// ==========================================
// Para girar o robô no próprio eixo (skid steer): um lado roda para frente, o outro para trás.
void girarDireita() {
  moverRobo(passosPara90Graus, HIGH, HIGH);
}
void girarEsquerda() {
  moverRobo(passosPara90Graus, LOW, LOW);
}