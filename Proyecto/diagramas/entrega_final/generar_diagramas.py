"""Diagramas del anteproyecto. Requiere Python 3 y Pillow (pip install Pillow).
Ejecutar: python generar_diagramas.py
Las imágenes PNG se guardan en la carpeta entrega_final, a 300 dpi.
"""
from pathlib import Path
import math
import os
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parent / 'entrega_final'
OUT.mkdir(exist_ok=True)
S = 3
INK = '#202b36'
FILL = '#f2f5f7'

def font(size, bold=False):
    names = ([str(Path(os.environ.get('WINDIR', 'C:/Windows')) / 'Fonts' / ('arialbd.ttf' if bold else 'arial.ttf')),
              '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf' if bold else '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'])
    for name in names:
        if Path(name).exists():
            return ImageFont.truetype(name, int(size*S))
    return ImageFont.truetype('DejaVuSans.ttf', int(size*S))

class Diagram:
    def __init__(self, title, h, subtitle=''):
        self.im = Image.new('RGB', (1000*S, h*S), 'white')
        self.d = ImageDraw.Draw(self.im)
        self.text(500, 38, title, 27, True)
        if subtitle: self.text(500, 77, subtitle, 17)

    def text(self, x, y, t, size=20, bold=False):
        self.d.multiline_text((x*S,y*S), t, font=font(size,bold), fill=INK,
                              anchor='mm', align='center', spacing=5*S)

    def box(self, x, y, t, w=290, h=76, kind='process', size=20):
        b = ((x-w/2)*S,(y-h/2)*S,(x+w/2)*S,(y+h/2)*S)
        if kind == 'decision':
            self.d.polygon([(x*S,(y-h/2)*S),((x+w/2)*S,y*S),(x*S,(y+h/2)*S),((x-w/2)*S,y*S)], fill='white', outline=INK, width=2*S)
        elif kind == 'end':
            self.d.rounded_rectangle(b, radius=27*S, fill=FILL, outline=INK, width=2*S)
        elif kind == 'connector':
            self.d.ellipse(b, fill=FILL, outline=INK, width=2*S)
        else:
            self.d.rectangle(b, fill=FILL, outline=INK, width=2*S)
        self.text(x,y,t,size)

    def arrow(self, pts, label=None, at=None, both=False):
        p = [(int(x*S),int(y*S)) for x,y in pts]
        self.d.line(p, fill=INK, width=2*S, joint='curve')
        def head(a,b):
            angle=math.atan2(b[1]-a[1],b[0]-a[0])
            self.d.polygon([b,(b[0]-12*S*math.cos(angle-.42),b[1]-12*S*math.sin(angle-.42)),(b[0]-12*S*math.cos(angle+.42),b[1]-12*S*math.sin(angle+.42))],fill=INK)
        head(p[-2],p[-1])
        if both: head(p[1],p[0])
        if label:
            x,y=at
            f=font(17)
            bb=self.d.textbbox((x*S,y*S),label,font=f,anchor='mm')
            self.d.rectangle((bb[0]-5*S,bb[1]-3*S,bb[2]+5*S,bb[3]+3*S), fill='white')
            self.d.text((x*S,y*S),label,font=f,fill=INK,anchor='mm')

    def conn(self,x,y,letter='A'):
        self.box(x,y,letter,44,44,'connector')

    def save(self,name):
        path=OUT/name
        self.im.save(path,dpi=(300,300))
        return path

def arquitectura():
    d=Diagram('Arquitectura del sistema',860,'Una mesa compartida · hasta cinco jugadores')
    d.box(500,210,'Proceso padre / master\nMazo, manos, turnos y resultados\nselect(): escucha y canales IPC',700,160,size=23)
    for x,n in [(190,'1'),(500,'2'),(810,'N')]:
        d.box(x,455,f'Proceso hijo {n}\nReenvío de mensajes',260,95)
        d.box(x,705,f'Cliente {n}\nInterfaz ncurses',260,95)
        d.arrow([(x,290),(x,407)],'IPC: socketpair()', (x,347),both=True)
        d.arrow([(x,503),(x,657)],'Red: TCP',(x,580),both=True)
    d.text(500,815,'Las flechas dobles representan comunicación en ambos sentidos.',18)
    return d.save('01_arquitectura.png')

def servidor():
    d=Diagram('Flujo del servidor principal',1430,'Proceso padre')
    d.box(500,135,'Inicio',180,54,'end')
    d.box(500,245,'Inicializar la mesa\nConfigurar socket de escucha',420,86)
    d.arrow([(500,162),(500,202)])
    d.conn(200,360)
    d.box(500,360,'Esperar actividad con select()',410,70)
    d.arrow([(500,288),(500,325)])
    d.arrow([(222,360),(295,360)])
    d.box(500,495,'¿Cerrar el\nservidor?',260,120,'decision')
    d.arrow([(500,395),(500,435)])
    d.box(830,495,'Cerrar canales\ny esperar a los hijos',260,86)
    d.arrow([(630,495),(700,495)],'Sí',(665,475))
    d.box(830,610,'Fin',160,54,'end')
    d.arrow([(830,538),(830,583)])
    d.box(500,650,'Identificar el evento',300,68)
    d.arrow([(500,555),(500,616)],'No',(522,581))
    for x,label in [(180,'Conexión'),(500,'Mensaje'),(820,'Desconexión')]:
        d.arrow([(500,684),(500,735),(x,735),(x,804)],label,(x,775))
    d.box(180,860,'Aceptar conexión\nCrear IPC e hijo',280,112)
    d.box(500,860,'Interpretar y validar\nla solicitud',280,112)
    d.box(820,860,'Retirar al jugador\ny liberar el canal',280,112)
    d.box(180,1025,'Registrar al jugador\no comunicar rechazo',280,112)
    d.box(500,1025,'Aplicar la acción\no informar el error',280,112)
    d.box(820,1025,'Ajustar el turno\nsi corresponde',280,112)
    for x in [180,500,820]: d.arrow([(x,916),(x,969)])
    d.box(500,1210,'Enviar respuestas y cambios de estado\na los clientes que corresponda',600,88)
    for x in [180,500,820]: d.arrow([(x,1081),(x,1125),(500,1125),(500,1166)])
    d.conn(500,1320)
    d.arrow([(500,1254),(500,1298)])
    d.text(500,1380,'A: volver a esperar actividad. Se muestra un evento por recorrido.',17)
    return d.save('02_flujo_servidor.png')

def hijo():
    d=Diagram('Flujo del proceso hijo',1110,'Puente entre el cliente y el proceso padre')
    d.box(500,135,'Inicio',180,54,'end')
    d.box(500,235,'Cerrar los descriptores\nque no utiliza',400,80)
    d.arrow([(500,162),(500,195)])
    d.box(500,355,'Esperar actividad\nen TCP y en el canal IPC',400,84)
    d.conn(190,355)
    d.arrow([(212,355),(300,355)])
    d.arrow([(500,275),(500,313)])
    for x,label in [(180,'Datos del cliente'),(500,'Datos del padre'),(820,'Cierre o error')]:
        d.arrow([(500,397),(500,465),(x,465),(x,545)],label,(x,509))
    d.box(180,595,'Reenviar la solicitud\nal proceso padre',280,100)
    d.box(500,595,'Reenviar la respuesta\nal cliente',280,100)
    d.box(820,595,'Avisar al padre\nsi el canal está abierto',280,100)
    for x in [180,500]:
        d.conn(x,755)
        d.arrow([(x,645),(x,733)])
    d.box(820,755,'Cerrar TCP e IPC',280,76)
    d.arrow([(820,645),(820,717)])
    d.box(820,895,'Fin del hijo',200,54,'end')
    d.arrow([(820,793),(820,868)])
    d.text(500,1015,'A: volver a esperar actividad. Los envíos pendientes se completan\ncuando el canal permite escribir; un error lleva al cierre.',18)
    return d.save('03_flujo_hijo.png')

def cliente():
    d=Diagram('Flujo del cliente',1530,'Conexión, registro e interacción con el jugador')
    d.box(420,130,'Inicio',170,54,'end')
    d.box(420,235,'Leer dirección y nombre\nIntentar la conexión TCP',380,80)
    d.arrow([(420,157),(420,195)])
    d.box(420,380,'¿Se pudo\nconectar?',270,120,'decision')
    d.arrow([(420,275),(420,320)])
    d.box(420,550,'Iniciar ncurses\ny solicitar el registro',380,84)
    d.arrow([(420,440),(420,508)],'Sí',(443,475))
    d.box(420,700,'¿Registro\naceptado?',270,120,'decision')
    d.arrow([(420,592),(420,640)])
    d.box(790,535,'Mostrar el motivo\ny cerrar los recursos\nque estén abiertos',300,120)
    d.arrow([(555,380),(790,380),(790,475)],'No',(659,360))
    d.arrow([(555,700),(790,700),(790,595)],'No',(659,680))
    d.box(420,880,'Esperar datos del servidor\no una tecla del usuario',380,84)
    d.arrow([(420,760),(420,838)],'Sí',(443,800))
    d.conn(130,880)
    d.arrow([(152,880),(230,880)])
    for x,label in [(180,'Datos'),(500,'Jugada'),(820,'Salida o cierre')]:
        d.arrow([(420,922),(420,970),(x,970),(x,1055)],label,(x,1010))
    d.box(180,1100,'Actualizar la pantalla',280,90)
    d.box(500,1100,'Enviar el comando',280,90)
    d.box(820,1100,'Cerrar la conexión\ny restaurar la terminal',280,90)
    for x in [180,500]:
        d.conn(x,1240)
        d.arrow([(x,1145),(x,1218)])
    d.box(820,1300,'Fin',160,54,'end')
    d.arrow([(820,1145),(820,1273)])
    d.arrow([(940,535),(974,535),(974,1300),(900,1300)])
    d.text(500,1420,'A: volver a esperar datos o teclado. Un error de comunicación\nse trata como cierre de la conexión.',18)
    return d.save('04_flujo_cliente.png')

def ronda():
    d=Diagram('Desarrollo de una ronda',1880,'Lógica del juego en el proceso padre')
    d.box(370,132,'Solicitud de inicio',270,54,'end')
    d.box(370,255,'¿Hay jugadores\ny la mesa está libre?',380,130,'decision')
    d.arrow([(370,159),(370,190)])
    d.box(790,255,'Rechazar la solicitud',290,70)
    d.arrow([(560,255),(645,255)],'No',(601,233))
    d.box(790,365,'Esperar otra solicitud',290,54,'end')
    d.arrow([(790,290),(790,338)])
    d.box(370,435,'Mezclar y repartir dos vueltas\nCon 21 inicial, la mano termina',440,90,size=19)
    d.arrow([(370,320),(370,390)],'Sí',(392,353))
    d.box(370,585,'Dealer: primera carta oculta\ny segunda carta visible',440,80)
    d.arrow([(370,480),(370,545)])
    d.box(370,755,'¿Quedan jugadores\npor actuar?',330,120,'decision')
    d.arrow([(370,625),(370,695)])
    d.conn(100,755,'T')
    d.arrow([(122,755),(205,755)])
    d.box(790,895,'Revelar la carta oculta\ny resolver al dealer¹',290,90)
    d.arrow([(535,755),(790,755),(790,850)],'No',(640,735))
    d.conn(790,1020,'R')
    d.arrow([(790,940),(790,998)])
    d.box(370,930,'Asignar el siguiente turno\ny esperar una acción válida',430,90)
    d.arrow([(370,815),(370,885)],'Sí',(392,852))
    d.conn(100,1080,'J')
    d.box(370,1080,'¿Qué acción\nrealiza?',300,120,'decision')
    d.arrow([(122,1080),(220,1080)])
    d.arrow([(370,975),(370,1020)])
    d.box(750,1230,'Finalizar el turno',300,76)
    d.arrow([(520,1080),(750,1080),(750,1192)],'Plantarse',(633,1060))
    d.box(370,1250,'Repartir una carta\ny calcular el puntaje',360,80)
    d.arrow([(370,1140),(370,1210)],'Pedir',(401,1175))
    d.box(370,1405,'¿Puntaje igual\no mayor que 21?',340,120,'decision')
    d.arrow([(370,1290),(370,1345)])
    d.arrow([(540,1405),(750,1405),(750,1268)],'Sí',(630,1385))
    d.conn(100,1405,'J')
    d.arrow([(200,1405),(122,1405)],'No',(159,1382))
    d.conn(930,1340,'T')
    d.arrow([(900,1230),(965,1230),(965,1340),(952,1340)])
    d.conn(100,1640,'R')
    d.box(430,1640,'Comparar las manos\ny publicar los resultados',440,86)
    d.arrow([(122,1640),(210,1640)])
    d.box(815,1640,'Fin de la ronda',250,54,'end')
    d.arrow([(650,1640),(690,1640)])
    d.text(500,1760,'T: siguiente jugador. J: otra acción. R: resultados. Con 21 no se pide más.\n¹ Si queda alguna mano válida, el dealer pide hasta llegar a 17 o más.\nSe evalúan cartas recibidas, nunca las que quedan en el mazo.',17)
    return d.save('05_flujo_ronda.png')

def reparto():
    d=Diagram('Orden del reparto inicial',1120,'Ejemplo con tres jugadores; el mismo orden se aplica hasta cinco')
    d.text(500,145,'Primera vuelta',23,True)
    xs=[135,375,615,855]
    for i,x in enumerate(xs):
        t=f'Jugador {i+1}\n1.ª carta' if i<3 else 'Dealer\n1.ª carta oculta'
        d.box(x,235,t,210,95,size=19)
        if i<3: d.arrow([(x+105,235),(xs[i+1]-105,235)])
    d.arrow([(855,283),(855,330),(135,330),(135,440)])
    d.text(500,390,'Segunda vuelta',23,True)
    for i,x in enumerate(xs):
        t=f'Jugador {i+1}\n2.ª carta\nEvaluar su mano' if i<3 else 'Dealer\n2.ª carta visible'
        d.box(x,500,t,210,120,size=18)
        if i<3: d.arrow([(x+105,500),(xs[i+1]-105,500)])
    d.text(500,627,'Al recibir la segunda carta, cada jugador queda en uno de estos estados:',19)
    d.box(270,740,'21 puntos\nMano terminada\nNo puede pedir más',360,125,size=22)
    d.box(730,740,'Menos de 21 puntos\nEspera su turno\nPuede pedir o plantarse',360,125,size=22)
    d.box(500,940,'El dealer conserva oculta su primera carta\nhasta que terminen los jugadores.',740,90,size=23)
    d.text(500,1050,'Solo se evalúan las cartas entregadas. No se anticipan cartas del mazo.',19)
    return d.save('06_reparto_inicial.png')

def dealer_flujo():
    d=Diagram('Resolución del dealer',1490,'Se ejecuta cuando no quedan jugadores por actuar')
    d.box(380,140,'Fin de los turnos',300,54,'end')
    d.box(380,260,'Revelar la primera carta\ny calcular el puntaje del dealer',440,86)
    d.arrow([(380,167),(380,217)])
    d.box(380,420,'¿Queda algún jugador\nque no se haya pasado?',400,140,'decision',19)
    d.arrow([(380,303),(380,350)])
    d.conn(840,420,'R')
    d.arrow([(580,420),(818,420)],'No',(683,398))
    d.box(380,630,'¿Dealer con\nmenos de 17?',330,130,'decision')
    d.arrow([(380,490),(380,565)],'Sí',(405,523))
    d.conn(100,630,'D')
    d.arrow([(122,630),(215,630)])
    d.conn(840,630,'R')
    d.arrow([(545,630),(818,630)],'No',(683,608))
    d.box(380,825,'Repartir una carta al dealer\ny recalcular su puntaje',440,90)
    d.arrow([(380,695),(380,780)],'Sí',(405,732))
    d.conn(380,945,'D')
    d.arrow([(380,870),(380,923)])
    d.conn(100,1130,'R')
    d.box(500,1130,'Comparar cada mano con la del dealer\nDeterminar victoria, derrota o empate',620,100)
    d.arrow([(122,1130),(190,1130)])
    d.box(500,1280,'Publicar resultados\ny finalizar la ronda',420,86)
    d.arrow([(500,1180),(500,1237)])
    d.text(500,1410,'D: volver a comprobar el puntaje. R: resolver resultados.\nEl dealer se planta con 17 o más, incluido 17 blando.\nBlackjack natural vence a 21 con más cartas; dos naturales empatan.',18)
    return d.save('07_flujo_dealer.png')

def cronograma():
    d=Diagram('Planificación del anteproyecto',830,'Estimación de ocho semanas · sin fechas calendario asignadas')
    left, top, cell, rh=430,170,62,76
    tasks=[('Definición y protocolo',1,1),('Comunicación TCP',2,2),
           ('Procesos y canales IPC',3,3),('Atención de varios clientes',4,4),
           ('Reglas del juego',5,6),('Interfaz con ncurses',7,7),
           ('Pruebas finales y documentación',8,8)]
    d.text(220,top-30,'Etapa',21,True)
    for week in range(1,9):
        d.text(left+(week-.5)*cell,top-30,f'S{week}',19,True)
    for row,(label,a,b) in enumerate(tasks):
        y=top+row*rh
        d.text(220,y+rh/2,label,19)
        for col in range(8):
            d.d.rectangle((int((left+col*cell)*S),int(y*S),int((left+(col+1)*cell)*S),int((y+rh)*S)),fill='white',outline='#b5bdc5',width=S)
        d.d.rectangle((int((left+(a-1)*cell+7)*S),int((y+18)*S),int((left+b*cell-7)*S),int((y+rh-18)*S)),fill='#486477')
    d.text(500,755,'Se realizan pruebas parciales durante cada etapa.\nLas semanas 5 y 6 corresponden al desarrollo de las reglas.',18)
    return d.save('08_cronograma.png')

if __name__ == '__main__':
    paths=[arquitectura(),servidor(),hijo(),cliente(),ronda(),reparto(),dealer_flujo(),cronograma()]
    thumbs=[]
    for path in paths:
        im=Image.open(path)
        im.thumbnail((400,752))
        thumbs.append(im)
    sheet=Image.new('RGB',(1600,1600),'#dddddd')
    for i,im in enumerate(thumbs): sheet.paste(im,((i%4)*400,(i//4)*800))
    sheet.save(OUT/'vista_general.png')
    for path in paths: print(path)
