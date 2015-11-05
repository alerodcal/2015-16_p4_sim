/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <ns3/core-module.h>
#include <ns3/callback.h>
#include <ns3/packet.h>
#include "BitAlternante.h"
#include "CabEnlace.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BitAlternante");

BitAlternanteTx::BitAlternanteTx(Ptr<NetDevice> disp,
                                 Time           espera,
                                 uint32_t       tamPqt,
                                 uint8_t        tamTx)
{
  NS_LOG_FUNCTION (disp << espera << tamPqt << tamTx);

  // Inicializamos las variables privadas
  m_disp      = disp;
  m_esperaACK = espera;
  m_tamPqt    = tamPqt;
  m_tamTx     = tamTx;
  m_tx        = 0;
  m_ventIni   = 0;
  m_totalPqt  = 0;
}



void
BitAlternanteTx::ACKRecibido(Ptr<NetDevice>        receptor,
                             Ptr<const Packet>     recibido,
                             uint16_t              protocolo,
                             const Address &       desde,
                             const Address &       hacia,
                             NetDevice::PacketType tipoPaquete)
{
  NS_LOG_FUNCTION (receptor << recibido->GetSize () <<
                   std::hex << protocolo <<
                   desde << hacia << tipoPaquete);
  
  // Obtengo el valor del número de secuecia
  Ptr<Packet> copia = recibido->Copy();

  CabEnlace header;
  copia->RemoveHeader (header);
  uint8_t numSecuencia = header.GetSecuencia();

  NS_LOG_DEBUG ("Recibido ACK en nodo " << m_node->GetId() << " con "
                << (unsigned int) numSecuencia << ". La ventana es [" 
                << (unsigned int) (m_ventIni)%256 << "," << (m_ventIni + m_tamTx - 1)%256 << "].");

  // Comprobamos si el número de secuencia del ACK se corresponde con
  // el de secuencia del siguiente paquete a transmitir
  if(numSecuencia == (m_ventIni + 1)%256)
  {
      // Si es correcto desactivo el temporizador
      m_temporizador.Cancel();
      // Desplazamos la ventana
      m_ventIni = m_ventIni + 1;
      NS_LOG_DEBUG("La ventana se desliza a [" << (unsigned int) (m_ventIni)%256 
                    << "," << (m_ventIni + m_tamTx - 1)%256 << "].");

      // Si el siguiente numero de secuencia a transmitir
      // esta dentro de la ventana lo enviamos
      if(m_tx != m_ventIni + m_tamTx) 
      {
        // Se transmite un nuevo paquete
        EnviaPaquete();
        // Cambiamos el número de secuencia
        m_tx = m_tx + 1;
      }
  } 
  else
  {
    NS_LOG_ERROR("Recibido ACK inesperado. Se ha recibido ACK = " << numSecuencia << 
                  " y se esperaba ACK = " << (m_ventIni+1)%256);

    // Desactivamos el temporizador.
    m_temporizador.Cancel();
    // Reenviamos todos los paquetes de la ventana.
    VenceTemporizador();
  }
}


void
BitAlternanteTx::VenceTemporizador()
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_LOG_ERROR ("Se ha producido una retransmisión. " 
    << "Se reenvian los paquetes con numero de secuencia perteneciente al intervalo: ["
    << (unsigned int) (m_ventIni)%256 << "," << (m_ventIni + m_tamTx - 1)%256 << "].");

  for (m_tx = m_ventIni; m_tx != m_ventIni + m_tamTx; m_tx++)
  {
    // Se reenvia un paquete
    EnviaPaquete();
  }
}


void
BitAlternanteTx::EnviaPaquete()
{
  NS_LOG_FUNCTION_NOARGS ();

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
    m_temporizador=Simulator::Schedule(m_esperaACK,&BitAlternanteTx::VenceTemporizador,this);
}


BitAlternanteRx::BitAlternanteRx(Ptr<NetDevice> disp)
{
  NS_LOG_FUNCTION (disp);

  m_disp = disp;
  m_rx   = 0;
  m_totalPqtACK = 0;
}


void
BitAlternanteRx::PaqueteRecibido(Ptr<NetDevice>        receptor,
                                 Ptr<const Packet>     recibido,
                                 uint16_t              protocolo,
                                 const Address &       desde,
                                 const Address &       hacia,
                                 NetDevice::PacketType tipoPaquete)
{
  NS_LOG_FUNCTION (receptor << recibido->GetSize () <<
                   std::hex << protocolo <<
                   desde << hacia << tipoPaquete);

  // Obtengo el valor del número de secuecia
  Ptr<Packet> copia = recibido->Copy();

  CabEnlace header;
  copia->RemoveHeader (header);
  uint8_t numSecuencia = header.GetSecuencia();

  NS_LOG_DEBUG ("Recibido paquete en nodo " << m_node->GetId() << " con "
                << (unsigned int) numSecuencia);
  // Si el número de secuencia es correcto

  if (numSecuencia == m_rx) 
    // Si es correcto, cambio el bit
    // Los numeros de secuencia estaran en el rango
    // 0-255 y cuando llegue a 255 desbordara y se pondra a 0.
    m_rx = m_rx+1;
  // Transmito en cualquier caso un ACK
  EnviaACK();
}


void
BitAlternanteRx::EnviaACK()
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
