/* ===============================================================
   FLOWSENSE v3.0 - ESP32 DevKit V4 - Arduino ESP32 3.x
   Projeto de extensao - monitoramento local de fluxo de pessoas

   MODO ATUAL: NQ-G3 (um sensor, GPIO 32, alimentado em 3V3)
   MODO FUTURO: 2 E18-D80NK (GPIO 32 e 33)
   ATENCAO: E18 tipicamente precisa de 5 V e saida NPN coletor
   aberto; usar pull-up EXTERNO de 10k para 3V3 em cada OUT,
   confirmar o modelo e NUNCA injetar 5 V nos GPIOs do ESP32.

   Portal cativo mantido: Wi-Fi FlowSense-ESP32 / FlowSense2026
   Painel: http://192.168.4.1
   LittleFS: historico CSV persistente
   Preferences: local, capacidade, sessao, ocupacao inicial

   Primeira execucao: pode formatar particao LittleFS vazia.
   Versao preparada, NAO testada em hardware nesta sessao.
   =============================================================== */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <sys/time.h>
#include <time.h>

// ------------------ CONFIGURACOES EDITAVEIS --------------------
const char* WIFI_NOME = "FlowSense-ESP32";
const char* WIFI_SENHA = "FlowSense2026";
const int WIFI_CLIENTES = 10;
const bool USAR_DOIS_E18 = true;  // ALTERAR para true APENAS apos montar e validar os dois E18
const uint8_t PINO_A = 32;
const uint8_t PINO_B = 33;
const int NIVEL_ATIVO = LOW;       // NQ-G3 e E18 NPN ativos em LOW
const unsigned long ANTIRRUIDO_MS = 80;
const unsigned long LIBERAR_MS = 300;
const unsigned long PRAZO_SEQUENCIA_MS = 1800;
const char* ARQUIVO_LOG = "/movimentos.csv";
const uint32_t LIMITE_LOG_BYTES = 650000; // Limite conservador. Ajustar para particao LittleFS.
// ---------------------------------------------------------------

WebServer servidor(80);
DNSServer dns;
Preferences prefer;
IPAddress ip(192,168,4,1), mascara(255,255,255,0);
bool storageOK = false;
bool gravacaoOK = false;
String nomeLocal = "Sala de aula";
uint32_t capacidade = 40;
uint32_t sessao = 1;
uint32_t ocupacaoInicial = 0;
uint32_t entradas = 0, saidas = 0, passagens = 0;
uint32_t registrosSessao = 0;

struct Evento { uint32_t epoch; uint32_t sessao; uint32_t ms; char tipo; String local; };
Evento recentes[12];
uint8_t totalRecentes = 0, proxRecente = 0;
struct Sensor {
  uint8_t pin;
  int bruto;
  int estavel;
  uint32_t mudouEm;
};
Sensor sensorA, sensorB;
bool pronto = false;
uint32_t livreDesde = 0;
uint8_t estadoPassagem = 2; // 0=aguardando, 1=sequencia, 2=aguardando liberar
bool primeiroFoiA = true;
uint32_t sequenciaEm = 0;
uint32_t ambosLivresDesde = 0;

uint32_t horaEpoch() {
  time_t t = time(nullptr);
  return t >= 1700000000 ? (uint32_t)t : 0;
}
uint32_t ocupacaoAtual() {
  int64_t n = (int64_t)ocupacaoInicial + entradas - saidas;
  return (uint32_t)(n < 0 ? 0 : n);
}
String jsonSeguro(const String& in) {
  String r; r.reserve(in.length() + 12);
  for (size_t i=0; i<in.length(); i++) {
    char c = in[i];
    if (c=='"' || c=='\\') { r+='\\'; r+=c; }
    else if (c=='\r' || c=='\n' || (uint8_t)c<32) r+=' ';
    else r+=c;
  }
  return r;
}
void responderJSON(int code,const String& s) {
  servidor.sendHeader("Cache-Control", "no-store");
  servidor.send(code,"application/json; charset=utf-8",s);
}
void falha(int code, const char* msg) {responderJSON(code, String("{\"erro\":\"")+msg+"\"}");}
void guardarRecente(const Evento &e) {
  recentes[proxRecente] = e;
  proxRecente = (proxRecente + 1) % 12;
  if (totalRecentes < 12) totalRecentes++;
}
char tipoReal(const String &s) {
  if(s=="E") return 'E';
  if(s=="S") return 'S';
  return 'P';
}
String nomeTipo(char tipo){return tipo=='E'?"entrada":tipo=='S'?"saida":"passagem";}

bool parseEvento(String linha, Evento &e) {
  linha.trim();
  int a=linha.indexOf(','), b=linha.indexOf(',',a+1), c=linha.indexOf(',',b+1);
  if(a<1 || b<0 || c<0) return false;
  e.epoch=(uint32_t)strtoul(linha.substring(0,a).c_str(),nullptr,10);
  e.sessao=(uint32_t)strtoul(linha.substring(a+1,b).c_str(),nullptr,10);
  e.tipo=tipoReal(linha.substring(b+1,c));
  int d=linha.indexOf(',',c+1);
  e.ms=(uint32_t)strtoul(linha.substring(c+1,d<0?linha.length():d).c_str(),nullptr,10);
  e.local=d<0?String("Local nao informado"):linha.substring(d+1);
  return true;
}
void restaurarHistorico() {
  entradas=saidas=passagens=registrosSessao=0;
  totalRecentes=proxRecente=0;
  if(!storageOK) return;
  File arquivo=LittleFS.open(ARQUIVO_LOG,FILE_READ);
  if(!arquivo) return;
  while(arquivo.available()) {
    Evento e;
    if(!parseEvento(arquivo.readStringUntil('\n'),e)) continue;
    guardarRecente(e);
    if(e.sessao!=sessao) continue;
    registrosSessao++;
    if(e.tipo=='E') entradas++;
    else if(e.tipo=='S') saidas++;
    else passagens++;
  }
  arquivo.close();
}

bool registrarEvento(char tipo) {
  if(!storageOK || !gravacaoOK) return false;
  Evento e = {horaEpoch(),sessao,millis(),tipo,nomeLocal};
  File arquivo=LittleFS.open(ARQUIVO_LOG,FILE_APPEND);
  if(!arquivo){gravacaoOK=false;return false;}
  size_t tamanhoMaximo = min((size_t)LIMITE_LOG_BYTES, LittleFS.totalBytes()>80000?LittleFS.totalBytes()-80000:(size_t)16000);
  if(arquivo.size()>=tamanhoMaximo){arquivo.close();gravacaoOK=false;Serial.println("ERRO: limite de historico atingido; exporte seus dados.");return false;}
  String linha=String(e.epoch)+","+String(e.sessao)+","+String(e.tipo)+","+String(e.ms)+","+e.local+"\n";
  size_t escritos=arquivo.print(linha);
  arquivo.flush(); arquivo.close();
  if(escritos!=linha.length()){gravacaoOK=false;Serial.println("ERRO de escrita em LittleFS");return false;}
  guardarRecente(e);
  registrosSessao++;
  if(tipo=='E') entradas++;
  else if(tipo=='S') saidas++;
  else passagens++;
  Serial.printf("FlowSense: %s | sessao %lu | UTC %lu\n",nomeTipo(tipo).c_str(),(unsigned long)sessao,(unsigned long)e.epoch);
  return true;
}

void iniciarSensor(Sensor &s,uint8_t pin) {
  s.pin=pin;
  // Pull-up interno para NQ-G3 (3V3). No E18, usar pull-up EXTERNO 10k a 3V3.
  pinMode(pin,INPUT_PULLUP);
  s.bruto=digitalRead(pin);s.estavel=s.bruto;s.mudouEm=millis();
}
void atualizarSensor(Sensor &s,uint32_t agora) {
  int valor=digitalRead(s.pin);
  if(valor!=s.bruto){s.bruto=valor;s.mudouEm=agora;}
  if(valor!=s.estavel && (uint32_t)(agora-s.mudouEm)>=ANTIRRUIDO_MS)s.estavel=valor;
}
void lerContagem(){
  uint32_t agora=millis();
  atualizarSensor(sensorA,agora);
  bool a=(sensorA.estavel==NIVEL_ATIVO);
  if(!USAR_DOIS_E18){
    if(!a){if(livreDesde==0)livreDesde=agora;if(!pronto && (uint32_t)(agora-livreDesde)>=LIBERAR_MS)pronto=true;}
    else {livreDesde=0;if(pronto){registrarEvento('P');pronto=false;}}
    return;
  }
  atualizarSensor(sensorB,agora);
  bool b=(sensorB.estavel==NIVEL_ATIVO);
  switch(estadoPassagem){
    case 0: // somente um sensor inicia a sequencia
      if(a && b){estadoPassagem=2;ambosLivresDesde=0;}
      else if(a != b){primeiroFoiA=a;sequenciaEm=agora;estadoPassagem=1;}
      break;
    case 1: // segundo sensor confirma sentido, com timeout
      if((uint32_t)(agora-sequenciaEm)>PRAZO_SEQUENCIA_MS){estadoPassagem=2;ambosLivresDesde=0;}
      else if(primeiroFoiA?b:a){registrarEvento(primeiroFoiA?'E':'S');estadoPassagem=2;ambosLivresDesde=0;}
      break;
    case 2: // so iniciar outra pessoa depois que ambos liberarem
      if(!a && !b){if(ambosLivresDesde==0)ambosLivresDesde=agora;
        if((uint32_t)(agora-ambosLivresDesde)>=LIBERAR_MS)estadoPassagem=0;
      } else ambosLivresDesde=0;
      break;
  }
}

// HTML separado para impedir que o preprocessador Arduino interprete JavaScript como C++.
#include "FlowSense_Dashboard.h"


void responderStatus(){
  String json="{\"dual\":"+String(USAR_DOIS_E18?"true":"false");
  json+=",\"local\":\""+jsonSeguro(nomeLocal)+"\"";
  json+=",\"capacidade\":"+String(capacidade);
  json+=",\"entradas\":"+String(entradas)+",\"saidas\":"+String(saidas);
  json+=",\"passagens\":"+String(USAR_DOIS_E18?entradas+saidas:passagens);
  json+=",\"ocupacao\":"+String(ocupacaoAtual());
  json+=",\"uptime\":"+String(millis());
  json+=",\"registros\":"+String(registrosSessao);
  json+=",\"clientes\":"+String(WiFi.softAPgetStationNum());
  json+=",\"relogio\":"+String(horaEpoch()?"true":"false");
  json+=",\"storage\":"+String((storageOK&&gravacaoOK)?"true":"false");
  json+=",\"detectando\":"+String((sensorA.estavel==NIVEL_ATIVO || (USAR_DOIS_E18&&sensorB.estavel==NIVEL_ATIVO))?"true":"false");
  json+=",\"recentes\":[";
  for(uint8_t i=0;i<totalRecentes;i++){
    int pos=(proxRecente+12-1-i)%12;
    if(i)json+=',';
    json+="{\"epoch\":"+String(recentes[pos].epoch)+",\"tipo\":\""+nomeTipo(recentes[pos].tipo)+"\"}";
  }
  json+="]}";
  responderJSON(200,json);
}
void receberHora(){
  if(!servidor.hasArg("epoch"))return falha(400,"Falta epoch");
  uint32_t epoch=(uint32_t)strtoul(servidor.arg("epoch").c_str(),nullptr,10);
  if(epoch<1700000000U || epoch>4102444800U)return falha(400,"Data invalida");
  time_t atual=time(nullptr);
  if(atual<1700000000 || llabs((long long)atual-(long long)epoch)>120){
    struct timeval tv={(time_t)epoch,0};settimeofday(&tv,nullptr);
  }
  responderJSON(200,"{\"ok\":true}");
}
void receberConfig(){
  if(!servidor.hasArg("local")||!servidor.hasArg("capacidade"))return falha(400,"Campos incompletos");
  String novo=servidor.arg("local");novo.trim();
  long cap=servidor.arg("capacidade").toInt();
  if(novo.length()<1||novo.length()>48||cap<1||cap>100000)return falha(400,"Valores invalidos");
  if(novo.startsWith("=")||novo.startsWith("+")||novo.startsWith("-")||novo.startsWith("@"))return falha(400,"Nome de local invalido");
  if(novo.indexOf(',')>=0||novo.indexOf(';')>=0||novo.indexOf('"')>=0||novo.indexOf('\n')>=0||novo.indexOf('\r')>=0)
    return falha(400,"Nome nao pode conter virgula, ponto e virgula ou aspas");
  if(!prefer.putString("local",novo) || !prefer.putUInt("capacidade",(uint32_t)cap))return falha(500,"Falha ao salvar NVS");
  nomeLocal=novo;capacidade=cap;
  responderJSON(200,"{\"ok\":true}");
}
void novaSessao(){
  if(!storageOK || !gravacaoOK)return falha(507,"Historico indisponivel");
  long inicial=servidor.hasArg("inicial")?servidor.arg("inicial").toInt():0;
  if(inicial<0||inicial>100000)return falha(400,"Ocupacao inicial invalida");
  uint32_t proxima=sessao+1;
  if(proxima==0)return falha(500,"Limite de sessoes");
  if(!prefer.putUInt("sessao",proxima) || !prefer.putUInt("inicial",(uint32_t)inicial))return falha(500,"Falha ao salvar sessao");
  sessao=proxima;ocupacaoInicial=USAR_DOIS_E18?(uint32_t)inicial:0;
  entradas=saidas=passagens=registrosSessao=0;
  pronto=false;livreDesde=0;estadoPassagem=2;ambosLivresDesde=0;
  responderJSON(200,"{\"ok\":true}");
}
void somatorioPorHora(){
  if(!servidor.hasArg("from")||!servidor.hasArg("to"))return falha(400,"Periodo ausente");
  uint32_t from=strtoul(servidor.arg("from").c_str(),nullptr,10);
  uint32_t to=strtoul(servidor.arg("to").c_str(),nullptr,10);
  if(to<=from || to-from>90000)return falha(400,"Periodo invalido");
  uint32_t horas[24]={0},semData=0;
  if(storageOK){
    File f=LittleFS.open(ARQUIVO_LOG,FILE_READ);
    if(f){while(f.available()){
      Evento e;if(!parseEvento(f.readStringUntil('\n'),e))continue;
      if(e.epoch==0){semData++;continue;}
      if(e.epoch>=from&&e.epoch<to && ((USAR_DOIS_E18&&e.tipo=='E')||(!USAR_DOIS_E18&&e.tipo=='P'))){
        uint32_t h=(e.epoch-from)/3600;if(h<24)horas[h]++;
      }
    }f.close();}
  }
  String json="{\"horas\":[";
  for(int i=0;i<24;i++){if(i)json+=',';json+=String(horas[i]);}
  json+= "],\"semData\":"+String(semData)+"}";
  responderJSON(200,json);
}
void exportarCSV(){
  if(!storageOK)return falha(507,"Historico indisponivel");
  bool tudo=servidor.arg("all")=="1";
  uint32_t from=strtoul(servidor.arg("from").c_str(),nullptr,10);
  uint32_t to=strtoul(servidor.arg("to").c_str(),nullptr,10);
  if(!tudo && (to<=from || to-from>120000000))return falha(400,"Intervalo invalido");
  File arquivo=LittleFS.open(ARQUIVO_LOG,FILE_READ);
  if(!arquivo)return falha(500,"Nao foi possivel abrir historico");
  servidor.setContentLength(CONTENT_LENGTH_UNKNOWN);
  servidor.sendHeader("Cache-Control","no-store");
  servidor.sendHeader("Content-Disposition","attachment; filename=FlowSense_Relatorio.csv");
  servidor.send(200,"text/csv; charset=utf-8","");
  servidor.sendContent("\xEF\xBB\xBF"); // BOM UTF-8 para Excel
  servidor.sendContent("Data e hora;Evento;Sessao;Local;Uptime do evento (ms)\r\n");
  String bloco;bloco.reserve(1100);
  while(arquivo.available()){
    Evento e;if(!parseEvento(arquivo.readStringUntil('\n'),e))continue;
    if(!tudo&&(e.epoch==0||e.epoch<from||e.epoch>=to))continue;
    String registro;
    if(e.epoch){ // UTC ISO 8601; Excel pode converter em hora local conforme configuracao
      time_t t=e.epoch;struct tm utc;gmtime_r(&t,&utc);
      char dt[32];strftime(dt,sizeof(dt),"%Y-%m-%dT%H:%M:%SZ",&utc);
      registro=String(dt);
    }else registro="Sem data";
    registro+=';';registro+=nomeTipo(e.tipo);registro+=';';registro+=String(e.sessao);
    registro+=';';registro+=e.local;registro+=';';registro+=String(e.ms);registro+="\r\n";
    if(bloco.length()+registro.length()>950){servidor.sendContent(bloco);bloco="";delay(0);}
    bloco+=registro;
  }
  arquivo.close();
  if(bloco.length())servidor.sendContent(bloco);
  servidor.sendContent("");
}
void redirecionar(){servidor.sendHeader("Location","http://192.168.4.1/",true);servidor.send(302,"text/plain","");}

void setup(){
  Serial.begin(115200);
  iniciarSensor(sensorA,PINO_A);
  if(USAR_DOIS_E18)iniciarSensor(sensorB,PINO_B);
  if(!prefer.begin("flowsense",false))Serial.println("ERRO: NVS indisponivel");
  nomeLocal=prefer.getString("local","Sala de aula");
  capacidade=prefer.getUInt("capacidade",40);
  sessao=prefer.getUInt("sessao",1);
  ocupacaoInicial=prefer.getUInt("inicial",0);
  // Na PRIMEIRA execucao, prepara a particao LittleFS vazia.
  // Depois disso, NUNCA formata automaticamente o historico existente.
  storageOK=LittleFS.begin(false);
  if(!storageOK && !prefer.getBool("fsInit",false)) {
    Serial.println("Inicializando LittleFS pela primeira vez...");
    storageOK=LittleFS.begin(true);
  }
  if(storageOK) prefer.putBool("fsInit",true);
  if(storageOK && !LittleFS.exists(ARQUIVO_LOG)){
    File f=LittleFS.open(ARQUIVO_LOG,FILE_WRITE);storageOK=(bool)f;if(f)f.close();
  }
  gravacaoOK=storageOK;
  restaurarHistorico();
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ip,ip,mascara);
  WiFi.softAP(WIFI_NOME,WIFI_SENHA,1,false,WIFI_CLIENTES);
  dns.start(53,"*",ip);
  servidor.on("/",HTTP_GET,[]{servidor.sendHeader("Cache-Control","no-store");servidor.send_P(200,"text/html; charset=utf-8",PAGINA);});
  servidor.on("/api/status",HTTP_GET,responderStatus);
  servidor.on("/api/hourly",HTTP_GET,somatorioPorHora);
  servidor.on("/api/export",HTTP_GET,exportarCSV);
  servidor.on("/api/config",HTTP_POST,receberConfig);
  servidor.on("/api/session",HTTP_POST,novaSessao);
  servidor.on("/api/time",HTTP_POST,receberHora);
  // MANTER O COMPORTAMENTO DO PORTAL CATIVO ANTERIOR
  servidor.on("/generate_204",HTTP_GET,redirecionar);
  servidor.on("/gen_204",HTTP_GET,redirecionar);
  servidor.on("/hotspot-detect.html",HTTP_GET,[]{servidor.send_P(200,"text/html; charset=utf-8",PAGINA);});
  servidor.on("/connecttest.txt",HTTP_GET,redirecionar);
  servidor.on("/fwlink",HTTP_GET,redirecionar);
  servidor.onNotFound(redirecionar);
  servidor.begin();
  Serial.println("FlowSense v3.0 iniciado!");
  Serial.print("Wi-Fi: ");Serial.println(WIFI_NOME);
  Serial.print("Dashboard: http://");Serial.println(WiFi.softAPIP());
  Serial.println(USAR_DOIS_E18?"Modo 2 E18-D80NK":"Modo 1 NQ-G3");
  if(!storageOK)Serial.println("AVISO: Falha LittleFS. Verifique a particao de flash.");
}
void loop(){
  dns.processNextRequest();
  servidor.handleClient();
  lerContagem();
  delay(2);
}
