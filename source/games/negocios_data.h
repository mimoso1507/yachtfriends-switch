// Generado desde www/g/negocios.js
#pragma once
namespace nd {
struct Sq { char t; int g; const char* n; const char* s; const char* e; int price; int rent[6]; int amt; char d; };
static const Sq BOARD[40]={
{'g',-1,"Salida","GO","🏁",0,{0,0,0,0,0,0},0,0},
{'p',0,"Callejón del Farol","Far","Far",60,{2,10,30,90,160,250},0,0},
{'c',-1,"Comunidad","👥","👥",0,{0,0,0,0,0,0},0,'C'},
{'p',0,"Pasaje Los Ceibos","Cei","Cei",60,{4,20,60,180,320,450},0,0},
{'x',-1,"Impuesto a las ganancias","💸","💸",0,{0,0,0,0,0,0},200,0},
{'t',-1,"Terminal Norte","🚂","🚂",200,{0,0,0,0,0,0},0,0},
{'p',1,"Costanera Río Verde","Cos","Cos",100,{6,30,90,270,400,550},0,0},
{'c',-1,"Suerte","🍀","🍀",0,{0,0,0,0,0,0},0,'S'},
{'p',1,"Calle Los Pinos","Pin","Pin",100,{6,30,90,270,400,550},0,0},
{'p',1,"Barrio La Ribera","Rib","Rib",120,{8,40,100,300,450,600},0,0},
{'j',-1,"Cárcel / Visita","⛓","⛓",0,{0,0,0,0,0,0},0,0},
{'p',2,"Plaza del Tamarindo","Tam","Tam",140,{10,50,150,450,625,750},0,0},
{'u',-1,"Usina Eléctrica","⚡","⚡",150,{0,0,0,0,0,0},0,0},
{'p',2,"Calle del Molino","Mol","Mol",140,{10,50,150,450,625,750},0,0},
{'p',2,"Avenida Las Palmas","Pal","Pal",160,{12,60,180,500,700,900},0,0},
{'t',-1,"Terminal Este","🚂","🚂",200,{0,0,0,0,0,0},0,0},
{'p',3,"Barrio Cerro Alto","Cer","Cer",180,{14,70,200,550,750,950},0,0},
{'c',-1,"Comunidad","👥","👥",0,{0,0,0,0,0,0},0,'C'},
{'p',3,"Calle Ñandú","Ñan","Ñan",180,{14,70,200,550,750,950},0,0},
{'p',3,"Paseo del Cóndor","Cón","Cón",200,{16,80,220,600,800,1000},0,0},
{'f',-1,"Estacionamiento libre","🅿️","🅿️",0,{0,0,0,0,0,0},0,0},
{'p',4,"Avenida del Sol","Sol","Sol",220,{18,90,250,700,875,1050},0,0},
{'c',-1,"Suerte","🍀","🍀",0,{0,0,0,0,0,0},0,'S'},
{'p',4,"Calle Jacarandas","Jac","Jac",220,{18,90,250,700,875,1050},0,0},
{'p',4,"Boulevard Las Acacias","Aca","Aca",240,{20,100,300,750,925,1100},0,0},
{'t',-1,"Terminal Sur","🚂","🚂",200,{0,0,0,0,0,0},0,0},
{'p',5,"Villa Marina","Mar","Mar",260,{22,110,330,800,975,1150},0,0},
{'p',5,"Avenida Cumbre Blanca","Cum","Cum",260,{22,110,330,800,975,1150},0,0},
{'u',-1,"Aguas del Valle","💧","💧",150,{0,0,0,0,0,0},0,0},
{'p',5,"Jardines del Sur","Jar","Jar",280,{24,120,360,850,1025,1200},0,0},
{'J',-1,"Andá a la cárcel","👮","👮",0,{0,0,0,0,0,0},0,0},
{'p',6,"Barrio Las Orquídeas","Orq","Orq",300,{26,130,390,900,1100,1275},0,0},
{'p',6,"Calle del Puerto","Pue","Pue",300,{26,130,390,900,1100,1275},0,0},
{'c',-1,"Comunidad","👥","👥",0,{0,0,0,0,0,0},0,'C'},
{'p',6,"Avenida Los Quetzales","Que","Que",320,{28,150,450,1000,1200,1400},0,0},
{'t',-1,"Terminal Oeste","🚂","🚂",200,{0,0,0,0,0,0},0,0},
{'c',-1,"Suerte","🍀","🍀",0,{0,0,0,0,0,0},0,'S'},
{'p',7,"Paseo Los Volcanes","Vol","Vol",350,{35,175,500,1100,1300,1500},0,0},
{'x',-1,"Impuesto al lujo","💎","💎",0,{0,0,0,0,0,0},100,0},
{'p',7,"Mirador de las Nubes","Mir","Mir",400,{50,200,600,1400,1700,2000},0,0}
};
static const unsigned GCOL[8]={0x8b5a2b,0x7ec8e3,0xe05fa0,0xf39c3d,0xe0483e,0xf4d03f,0x2ecc71,0x3b6fd4};
static const int GHC[8]={50,50,100,100,150,150,200,200};
static const char* GNAME[8]={"Marrón","Celeste","Rosa","Naranja","Rojo","Amarillo","Verde","Azul"};
struct Cd { int t, a, to, h, H; const char* x; };
static const Cd CARD_S[]={
{0,0,0,0,0,"Avanzá hasta la Salida y cobrá ₡200."},
{0,0,39,0,0,"Avanzá hasta el Mirador de las Nubes."},
{0,0,24,0,0,"Avanzá hasta el Boulevard Las Acacias. Si pasás por la Salida, cobrás ₡200."},
{0,0,11,0,0,"Avanzá hasta la Plaza del Tamarindo. Si pasás por la Salida, cobrás ₡200."},
{0,0,5,0,0,"Viajá hasta la Terminal Norte. Si pasás por la Salida, cobrás ₡200."},
{1,0,0,0,0,"Avanzá hasta la terminal más cercana. Si tiene dueño, pagás el doble."},
{2,50,0,0,0,"El banco te paga un dividendo de ₡50."},
{2,150,0,0,0,"Tu inversión rinde frutos: cobrá ₡150."},
{5,0,0,0,0,"Salí de la cárcel gratis. Guardá esta tarjeta."},
{6,0,0,0,0,"Retrocedé 3 casillas."},
{4,0,0,0,0,"Andá directo a la cárcel. No pasás por la Salida."},
{7,0,0,25,100,"Reparaciones generales: ₡25 por casa y ₡100 por hotel."},
{3,15,0,0,0,"Multa por exceso de velocidad: pagá ₡15."},
{8,50,0,0,0,"Te eligen presidente del club: pagá ₡50 a cada jugador."}
};
static const int N_S=14;
static const Cd CARD_C[]={
{2,200,0,0,0,"Error del banco a tu favor: cobrá ₡200."},
{3,50,0,0,0,"Honorarios del médico: pagá ₡50."},
{2,45,0,0,0,"Vendés mercadería en la feria: cobrá ₡45."},
{5,0,0,0,0,"Salí de la cárcel gratis. Guardá esta tarjeta."},
{4,0,0,0,0,"Andá directo a la cárcel. No pasás por la Salida."},
{9,10,0,0,0,"Es tu cumpleaños: cada jugador te da ₡10."},
{2,20,0,0,0,"Te devuelven impuestos: cobrá ₡20."},
{2,100,0,0,0,"Vence tu seguro de vida: cobrá ₡100."},
{3,100,0,0,0,"Gastos de hospital: pagá ₡100."},
{3,50,0,0,0,"Cuota del colegio: pagá ₡50."},
{7,0,0,40,115,"Arreglos de la calle: ₡40 por casa y ₡115 por hotel."},
{9,25,0,0,0,"Rifa del barrio: cada jugador te da ₡25."},
{2,100,0,0,0,"Recibís una herencia: cobrá ₡100."},
{0,0,0,0,0,"Avanzá hasta la Salida y cobrá ₡200."}
};
static const int N_C=14;
}
