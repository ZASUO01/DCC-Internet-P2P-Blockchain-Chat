# DCC Internet P2P Blockchain Chat

Neste trabalho, você irá desenvolver um sistema de armazenamento de chats distribuído. O sistema funciona de forma par-a-par e utiliza um blockchain como mecanismo de verificação dos chats. O desenvolvimento do trabalho será dividido em três partes.

## Parte 0: Informações preliminares

Este trabalho pode ser implementado em qualquer linguagem de programação e utilizar qualquer função da biblioteca padrão da linguagem escolhida. Seu programa irá estabelecer diversas conexões de rede simultaneamente. Para controlar várias conexões, seu programa deverá utilizar múltiplas threads ou um mecanismo equivalente ao select().

Seu programa irá trocar informações com outros programas utilizando mensagens de rede com formato pré-definido. Mensagens são transmitidas pela rede como uma sequência ordenada de campos. Existem três tipos de campo: (1) inteiro sem sinal de 1 byte, (2) inteiro sem sinal de 4 bytes e (3) sequência de bytes. Os campos inteiros deverão ser transmitidos sempre em network byte order e no tamanho indicado. As sequências de caracteres dos chats armazenados usarão codificação ASCII (1 byte por caractere, sem acentos).

    Para criação de estruturas em C, utilize os tipos inteiros de tamanho fixo definidos no cabeçalho <stdint.h>. Em Python, utilize o módulo struct.

Todas as conexões a pares remotos devem ser iniciadas na porta 51511. Seu programa deve dar bind() nesta porta para receber novas conexões. Como a porta de conexão é fixa, apenas um programa pode ser executado por endereço IP. Dica: No Linux, você pode adicionar vários IPs à interface loopback usando, por exemplo, ip addr add 127.0.0.2/8 dev lo, o que pode facilitar o teste e depuração do seu programa.

O professor irá disponibilizar uma implementação própria do programa em um servidor cujo endereço IP será informado na página do curso, para que você possa testar seu programa com outra implementação.

    Se for testar seu programa a partir de uma rede com NAT (como na maioria das redes domésticas), lembre-se de encaminhar a porta 51511 no NAT para o computador onde seu programa está executando. Se a rede que for utilizar possuir CGNAT (Carrier-Grade NAT), então não será possível receber conexões externas.

Parte 1: Identificação de pares e estabelecimento da rede P2P

Ao entrar no sistema, seu programa irá se conectar a um par previamente conhecido que já está no sistema. Após o estabelecimento da conexão com um par do sistema, seu programa irá requisitar e obter uma lista de outros pares no sistema. Após receber a lista de outros pares, seu programa deverá se conectar a todos os pares aos quais ainda não está conectado. Para esta parte do trabalho, seu programa deve implementar as mensagens PeerRequest e PeerList descritas abaixo.

    PeerRequest [0x1] (1 byte): Esta mensagem é composta por 1 byte de código de mensagem, que deve ter sempre o valor 0x1. Esta mensagem não possui outros campos. Ao receber esta mensagem, seu programa deve responder com uma mensagem PeerList.

    PeerList [0x2] (5 + 4xN bytes): Esta mensagem começa com 1 byte de código de mensagem, que deve ter sempre o valor 0x2. Após o código de mensagem, há um inteiro de 4 bytes informando a quantidade N de pares conhecidos. O restante da mensagem contém informações sobre os N pares. Para cada um dos N pares, a mensagem contém o endereço IP do par (equivalente a um inteiro de 4 bytes). Ao receber uma mensagem PeerList, seu programa deve se conectar a todos os pares aos quais ainda não está conectado. (Lembre-se: toda a comunicação usa uma porta única comum a todos os dispositivos da rede, que será informada na página do curso.)

Para garantir que todos os pares possuam informação sobre todos os outros pares e para facilitar a identificação de quais pares estão ativos, seu programa deve enviar mensagens PeerRequest para cada par conhecido a cada 5 segundos.

    Dica: Para receber e decodificar a mensagem, leia primeiro apenas 1 byte do soquete para saber qual o tipo da mensagem. Depois, leia e processe o restante da mensagem, que pode ter tamanho variável.

Parte 2: Histórico de chats

O seu programa deve manter um histórico de todos os chats conhecidos. Sempre que um par cria um chat (parte 3), ele é inserido no histórico de chats. Nesta parte, seu programa deve implementar duas novas mensagens:

    ArchiveRequest [0x3] (1 byte): Esta mensagem contém um inteiro de 1 byte com o valor 0x3. Esta mensagem não possui outros campos. Quando seu programa recebe uma mensagem deste tipo, ele deve responder com uma mensagem do tipo ArchiveResponse.

    ArchiveResponse [0x4] (5 bytes + chats): Esta mensagem começa com um inteiro de 1 byte contendo o valor 0x4. Em seguida, há um inteiro de 4 bytes contendo o número C de chats no histórico. A mensagem contém então uma quantidade variável de bytes para cada chat enviado (descrito abaixo). Ao receber uma mensagem ArchiveResponse seu programa deve primeiro verificar se esse novo histórico é válido (descrito abaixo). Se o histórico não for válido, seu programa deve ignorá-lo. Se o histórico for válido, seu programa deve comparar se o novo histórico contém mais chats do que o histórico anterior. Se contiver, deverá substituir o histórico anterior pelo novo. Ao ignorar um histórico inválido ou que contém menos chats que o histórico anterior, seu programa não deve desconectar o peer (mas você pode considerar enviar uma mensagem de notificação).

Chats (1 + N + 32 bytes): Os históricos são formados por uma sequência de chats. Cada chat começa com um inteiro de um byte que indica o número N de caracteres no chat. (Cada chat pode ter no máximo 255 caracteres.) Os N bytes seguintes contêm o texto do chat em formato ASCII. O inteiro N não deve contar o caractere de terminação de strings ('\0') utilizado pela linguagem C; o texto dos chats pode conter apenas caracteres alfanuméricos. Após os N+1 bytes iniciais, um chat contém mais 32 bytes divididos em dois campos: um código verificador de 16 bytes e um hash MD5 de 16 bytes.
Verificação de um histórico

Cada histórico é composto por uma sequência de chats. A sequência inteira de chats em um histórico é verificada computacionalmente. Um histórico é considerado válido se as condições abaixo forem satisfeitas:

    O hash MD5 do último chat deve começar com dois bytes zero.
    O hash MD5 do último chat deve ser igual ao hash MD5 calculado sobre uma sequência de bytes S. A sequência S é definida como a sequência de bytes dos últimos 20 chats no histórico (incluindo todos os 1 + N + 32 bytes de cada um dos últimos 20 chats), exceto os últimos 16 bytes do último chat (o hash MD5 do último chat).
    O histórico anterior, ou seja, o histórico sem a última mensagem, deve ser válido. Em outras palavras, os históricos de cada chat devem ser verificados recursivamente.

Se um histórico tem menos de 20 chats, todos seus chats devem ser considerados.

Note que, em média, apenas 1 em cada 65535 hashes MD5 possui os dois primeiros bytes iguais a zero. Para criar um chat e adicioná-lo ao histórico, seu programa deve minerar valores para o código verificador do seu chat até encontrar um valor que faça o MD5 calculado sobre a sequência S (formada pelo seu chat e pelos 19 chats precedentes) tenha os dois primeiros bytes iguais a zero. Em outras palavras, é preciso que seu programa gere códigos verificadores aleatoriamente e calcule o hash MD5 repetidamente até achar um hash MD5 com os dois primeiros bytes iguais a zero. Para que esse cálculo seja possível, os 16 bytes do hash MD5 do seu novo chat não são considerados durante o processo de mineração e de verificação do histórico (ou seja, os bytes do MD5 são considerados inexistentes).
Exemplo comentado de histórico com 5 mensagens

Abaixo vemos o dump hexadecimal de um histórico com 5 mensagens. A coluna mais à esquerda mostra um contador de bytes em base 16; note que cada linha tem 16 bytes. Abaixo de cada linha do hexdump apresentamos uma linha com os símbolos -=.>< para indicar partes específicas do dump:

00000000: 0000 0005 2363 756e 6861 4064 6363 2e75  ....#cunha@dcc.u
          --------- ==...........................
00000010: 666d 672e 6272 2072 6f6f 7420 6767 206e  fmg.br root gg n
          .......................................
00000020: 6f72 6576 2074 6878 e6bf 4f54 1640 32cf  orev thx..OT.@2.
          ................... >>>>>>>>>>>>>>>>>>>
00000030: a59a 1ab1 364b d468 0000 5f1e 839f 2f67  ....6K.h.._.../g
          >>>>>>>>>>>>>>>>>>> <<<<<<<<<<<<<<<<<<<
00000040: 4797 be4f 75d1 dab7 2463 756e 6861 4064  G..Ou...$cunha@d
          <<<<<<<<<<<<<<<<<<< ==.................
00000050: 6363 2e75 666d 672e 6272 2069 7a69 206d  cc.ufmg.br izi m
          .......................................
00000060: 6964 2069 7a69 206c 6966 6520 32b7 a60f  id izi life 2...
          ................................>>>>>>>
00000070: 720e b60c 4035 52bf 13f7 e9e4 df00 0015  r...@5R.........
          >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<
00000080: fe9a 50aa 9e33 a03b 9294 6e98 a606 6761  ..P..3.;..n...ga
          <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<== ....
00000090: 686f 6761 be9c bfd3 81a1 f2de 90c4 f2b2  hoga............
          ......... >>>>>>>>>>>>>>>>>>>>>>>>>>>>>
000000a0: 6af0 4f96 0000 2e68 cc65 451f b8ac 3299  j.O....h.eE...2.
          >>>>>>>>> <<<<<<<<<<<<<<<<<<<<<<<<<<<<<
000000b0: cd2d 6cc7 1663 756e 6861 4064 6363 2074  .-l..cunha@dcc t
          <<<<<<<<< ==...........................
000000c0: 6573 7420 6861 6e67 6f75 7455 c995 7513  est hangoutU..u.
          ...........................>>>>>>>>>>>>
000000d0: 949c 8249 5739 444e efdf b200 0035 5e52  ...IW9DN.....5^R
          >>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<
000000e0: 3443 b2a1 b131 e63a 2e53 7d08 6e6f 7661  4C...1.:.S}.nova
          <<<<<<<<<<<<<<<<<<<<<<<<<<<== .........
000000f0: 206d 7367 2a97 d3ea e253 ad20 e880 78e2   msg*....S. ..x.
          ......... >>>>>>>>>>>>>>>>>>>>>>>>>>>>>
00000100: 073d dd34 0000 9cbe 783b 2a1d e7f8 33b3  .=.4....x;*...3.
          >>>>>>>>> <<<<<<<<<<<<<<<<<<<<<<<<<<<<<
00000110: 78dc 5ef9
          <<<<<<<<<

--------: Indicador do número de chats no histórico (5)
==      : Primeiro byte de cada chat, contendo a contagem de caracteres
........: Bytes ASCII com o conteúdo da mensagem
>>>>>>>>: 16 bytes do código de verificação
<<<<<<<<: 16 bytes do checksum MD5 começando com 2 bytes zero

Parte 3: Envio de chats

Para enviar um chat, seu programa precisa apenas obter o histórico atual e criar um novo histórico anexando seu novo chat. Para isso, é necessário minerar um código verificador que inclua sua mensagem no novo histórico. Ao criar um novo histórico, seu programa deve disseminá-lo (para evitar que outros históricos proliferem antes) enviando mensagens ArchiveResponse para todos os parceiros.
Parte 4: Mensagens de notificação (opcional)

Ao detectar um erro (p.ex., erro ao decodificar uma mensagem recebida, um arquivo inválido) ou situação inesperada (p.ex., receber um histórico menor que o corrente), seu programa pode enviar uma mensagem de notificação para informar o peer do ocorrido, facilitando a resolução de problemas.

    NotificationMessage [0x5] (1 byte + 1 byte + N bytes): Esta mensagem é composta por 1 byte de código de mensagem, 1 byte especificando o número de caracteres na mensagem de notificação, e N caracteres adicionais com a mensagem. A mensagem de notificação deve estar codificada em ASCII.

Testes, Implantação e Composição de Grupos

Seu programa deve interoperar com a implementação de referência do professor e com programas de outros colegas. Para testar seu programa, você deve deixá-lo em operação na rede P2P construída pela implementação do professor e dos seus colegas.

Você precisará executar sua implementação em um dispositivo que não esteja atrás de um NAT. Para isso, vale utilizar um computador na rede da UFMG.
Entrega em grupo

Este trabalho pode ser realizado em duplas.

Alternativamente, você pode entregar o trabalho em um grupo de três alunos. Grupos de três alunos precisam executar a implementação do trabalho em um provedor de nuvem por um período contínuo de pelo menos 5 dias antes do prazo de entrega.
O que deve ser entregue

Seu programa deve inserir pelo menos um chat no histórico mantido pela implementação de referência do professor. (Na mensagem você deve deixar claro o nome dos integrantes do grupo.)
Pontos extras

Bugs reportados na implementação de referência do professor valem pontos extras, a serem definidos em função da gravidade ou impacto do bug.
Relatório

Cada grupo deve entregar um relatório descrevendo:

    A implementação do programa, incluindo uma descrição dos mecanismos utilizados para codificação, recebimento e envio de mensagens de rede. Informe qual biblioteca foi utilizada para fazer o cálculo do hash MD5 e como o processo de mineração foi implementado.
    Instruções para execução do seu programa.
    Um relato do estado do histórico no momento da submissão (indique pelo menos o número de mensagens e o conteúdo da última mensagem).
    O conteúdo (tamanho, mensagem ASCII, código de verificação e hash MD5) da mensagem enviada à blockchain identificando os integrantes do grupo.

