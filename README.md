# Domaci iz MSREAL - Simulacija fifo modula u kernelskoj memoriji
  Zadatak [nalaze](http://www.elektronika.ftn.uns.ac.rs/mikroracunarski-sistemi-za-rad-u-realnom-vremenu/wp-content/uploads/sites/99/2018/03/Doma%C4%87i-zadatak.pdf) da se napravi pamcenje od maksimum 16 integer vrednosti u fifo stilu memorije. Potrebno je omoguciti suspendovanje procesa pri punom ili praznom baferu, ispis, citanje, kao i citanje vise od jednog elementa odjednom. Omoguciti sigurnu operaciju na SMP sistemima koriscenjem semafora. Napisati kratku aplikaciju koja koristi dati kernel modul i upisuje/ispisuje iz char uredjaja.
## Upis u fifo
  Upis je realizovan na nacin da se prima kompletan string hex brojeva odvojen sa ";" a potom se u petlji funkcijom [strsep](https://www.kernel.org/doc/htmldocs/kernel-api/API-strsep.html) svakom iteracijom odvaja po jedan broj iz niza i pretvara u integer i u istom trenutku upise u bafer. Ukoliko nema dovoljno mesta za celi niz brojeva, upisuju se oni za koje ima mesta, a proces se suspenduje kada se bafer napuni.
## Citanje iz fifo-a
  Citanje (sa mogucnoscu citanja vise elemenata) je realizovano u petlji koja u zavisnosti od zahtevanog broja elemenata (u kodu batch), cita iz memorije clanove, formatira ih u string oblika hex broja, i dodaje u pripremni bafer. Svakom iteracijom(zavisno od batch) novi clan se dodaje, i tek na kraju se kopira celi niz i vraca korisniku. Terminalu se vraca 4*batch vrednost. Pozivanje `fifo_read()` funkcije dva puta od strane cat komande se resava na nacin da se svaki drugi poziv ignorise (u kodu secondPass);
## Suspendovanje procesa
  U slucaju prekoracenja opsega niza pri upisu ili citanja iz praznog niza, proces se stavlja na cekanje, sto je realizovano uslovom `wait_event_interruptible(readQueue,(pos>(batch-1)))` za read i `wait_event_interruptible(writeQueue,(pos<16))` za write funkciju. Proces koji je na cekanju ce biti nastavljen tek kada se steknu uslovi (upise ili procita elemenat) i pozove `wake_up_interruptible(&readQueue)` i `wake_up_interruptible(&readQueue)` za nastavak citanja i pisanja respektivno.
## Semafori
  Zbog mogucnosti simultanog pristupa kriticnim promenljivim, moguce su nezeljene pojave kao citanje/upis u nedozvoljeni segment memorije. Ovo se izbegava spustanjem semafora pri baratanju sa promenljivom pos (koja sluzi kao globalni iterator kroz fifo niz). Pri zavrsetku kriticnog dela koda, ili pri pozivanju `wait_event_interruptible()` potrebno je podignuti semafor kako bi drugi proces mogao nastaviti rad, ili probuditi trenutni ako je stavljen u queue. U kodu tu ulogu igra binarni semafor i funkcije koje ga koriste su `fifo_write()` i `fifo_read()`.
## Aplikacija
  Kratka aplikacija za demonstraciju rada modula. Preporucuje se posmatranje kernelskih poruka istovremeno, jer aplikacija biva suspendovana bez upozorenja u slucaju punog ili praznog bafera, dok su detalji o radu ispisani putem printk. Iz razloga sto sa secondPass ignorisemo svako drugo pozivanje `fifo_read()`, `fgets()` je pozvan duplo.
