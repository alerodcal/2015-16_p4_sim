/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <ns3/core-module.h>
#include <ns3/callback.h>
#include <ns3/packet.h>
#include "Enlace.h"
#include "CabEnlace.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Enlace");

Enlace::Enlace(Ptr<NetDevice> disp,
                                 Time           espera,
                                 uint32_t       tamPqt,
                                 uint8_t        tamTx)
{
  NS_LOG_FUNCTION (disp << espera << tamPqt << (unsigned int)tamTx);

  // Inicializamos las variables privadas
  m_disp      = disp;
  m_esperaACK = espera;
  m_tamPqt    = tamPqt;
  m_tamTx     = tamTx;
  m_tx        = 0;
  m_rx        = 0;
  m_ventIni   = 0;
  m_totalPqt  = 0;
  m_totalPqtACK = 0;
}

void
Enlace::ACKRecibido(uint8_t numSecuencia)
{
  NS_LOG_FUNCTION ((unsigned int)numSecuencia);

  NS_LOG_DEBUG ("Recibido ACK en nodo " << m_node->GetId() << " con "
                << (unsigned int) numSecuencia << ". La ventana es [" 
                << (unsigned int) (m_ventIni)%MODULO << "," << (unsigned int) (m_ventIni + m_tamTx - 1)%MODULO << "].");

  // Comprobamos si el número de secuencia del ACK se corresponde con
  // el de secuencia del siguiente paquete a transmitir
  if((numSecuencia != m_ventIni) && (Offset (m_tx) >= Offset (numSecuencia)))
  {
      // Si es correcto desactivo el temporizador
      m_temporizador.Cancel();
      // Desplazamos la ventana
      m_ventIni = numSecuencia;
      NS_LOG_DEBUG("La ventana se desliza a [" << (unsigned int) (m_ventIni)%MODULO 
                    << "," << (unsigned int) (m_ventIni + m_tamTx - 1)%MODULO << "].");

      // Si el siguiente numero de secuencia a transmitir
      // esta dentro de la ventana lo enviamos
      // ESTO SE COMPRUEBA EN ENVIA PKT
      //if(m_tx != m_ventIni + m_tamTx) 
      //{
        // Se transmite un nuevo paquete
        EnviaPaqueteDatos();
        // Cambiamos el número de secuencia
        // ESTO SE COMPRUEBA EN ENVIA PKT
        //m_tx = m_tx + 1;
      //}
  } 
  /*else
  {
    NS_LOG_DEBUG("Recibido ACK inesperado. Se ha recibido ACK = " << (unsigned int) numSecuencia << 
                  " y se esperaba ACK = " << (unsigned int) (m_ventIni+1)%MODULO);

    // Desactivamos el temporizador.
    m_temporizador.Cancel();
    // Reenviamos todos los paquetes de la ventana.
    VenceTemporizador();
  }*/
}

void
Enlace::DatoRecibido(uint8_t numSecuencia)
{
  NS_LOG_FUNCTION ((unsigned int)numSecuencia);

  NS_LOG_DEBUG ("Recibido paquete en nodo " << m_node->GetId() << " con "
                << (unsigned int) numSecuencia);

  if (numSecuencia == m_rx) 
    // Los numeros de secuencia estaran en el rango
    // 0-255 y cuando llegue a 255 desbordara y se pondra a 0.
    m_rx = m_rx+1;
  // Transmito en cualquier caso un ACK
  EnviaACK();
}

void 
Enlace::PaqueteRecibido(Ptr<NetDevice>        receptor,
                             Ptr<const Packet>     recibido,
                             uint16_t              protocolo,
                             const Address &       desde,
                             const Address &       hacia,
                             NetDevice::PacketType tipoPaquete)
{
  NS_LOG_FUNCTION (receptor << recibido->GetSize () <<
                   std::hex << protocolo <<
                   desde << hacia << tipoPaquete);

  // Obtengo una copia del paquete
  Ptr<Packet> copia = recibido->Copy();

  CabEnlace header;
  // Quitamos la cabecera del paquete y la guardamos en header.
  copia->RemoveHeader (header);
  // Obtenemos el tipo y el numero de secuencia.
  uint8_t tipo = header.GetTipo();
  uint8_t numSecuencia = header.GetSecuencia();

  if (tipo == 0)
  {
    // Paquete de datos recibido.
    DatoRecibido(numSecuencia);
  } 
  else 
  {
    // Paquete de ACK recibido
    ACKRecibido(numSecuencia);
  }
}

void
Enlace::VenceTemporizador()
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_LOG_DEBUG ("Se ha producido una retransmisión. " 
    << "Se reenvian los paquetes con numero de secuencia perteneciente al intervalo: ["
    << (unsigned int) (m_ventIni)%MODULO << "," << (m_ventIni + m_tamTx - 1)%MODULO << "].");

  // ESTE BUCLE SE HACE EN ENVIA PKT
  //for (m_tx = m_ventIni; m_tx != m_ventIni + m_tamTx; m_tx++)
  //{
    // Inicia ventana de transmision
    m_tx = m_ventIni;
    // Se reenvia un paquete
    EnviaPaqueteDatos();
  //}
}


void
Enlace::EnviaPaqueteDatos()
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_LOG_DEBUG((unsigned int)m_tx << " " << (unsigned int)m_ventIni << " " << (unsigned int)m_tamTx << " "<< MODULO);

  // Enviamos si tenemos credito.
  for(; ((uint32_t)m_tamTx - Offset (m_tx)) > 0; m_tx++)
  {
    Ptr<Packet> paquete = Create<Packet> (m_tamPqt);

    //Formamos la cabecera
    CabEnlace header;
    header.SetData (0, m_tx);

    //Añadimos la cabecera
    paquete->AddHeader (header);

    // Envío el paquete
    m_node->GetDevice(0)->Send(paquete, m_disp->GetAddress(), 0x0800);

    NS_LOG_INFO ("   Solicitado envio de paquete de " << paquete->GetSize () <<
                 " octetos en nodo " << m_node->GetId() <<
                 " con " << (unsigned int) m_tx <<
                 " en " << Simulator::Now());

    //Aumentamos el total de paquetes transmitidos
    m_totalPqt++;

    // Programo el temporizador si no esta ya en marcha
    if (m_esperaACK != 0 && m_temporizador.IsRunning() == false)
      m_temporizador=Simulator::Schedule(m_esperaACK,&Enlace::VenceTemporizador,this);
  }
}

void
Enlace::EnviaACK()
{
  NS_LOG_FUNCTION_NOARGS ();
  Ptr<Packet> p = Create<Packet> (1);

  //Formamos la cabecera
  CabEnlace header;
  header.SetData (1, m_rx);
  //Añadimos la cabecera
  p->AddHeader (header);

  m_node->GetDevice(0)->Send(p, m_disp->GetAddress(), 0x0800);

  NS_LOG_DEBUG ("Transmitido ACK de " << p->GetSize () <<
                " octetos en nodo " << m_node->GetId() <<
                " con " << (unsigned int) m_rx <<
                " en " << Simulator::Now());

  //Aumentamos el numero de ACKs transmitidos.
  m_totalPqtACK++;
}

// Calcula valor referido al extremo izquierdo de la ventana
uint32_t Enlace::Offset    (uint32_t valor)  { return (valor + (uint32_t)MODULO - (uint32_t)m_ventIni) % (uint32_t)MODULO; }