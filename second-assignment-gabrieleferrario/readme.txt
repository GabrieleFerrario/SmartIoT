Assignment2: Gabriele Ferrario
Per eseguire il codice occorre aver installato Python e si deve creare un venv con le librerie specificate nel file requirements.txt.
La web app una volta attivato venv si può eseguire tramite il comando: python app.py

E' stato caricato un video in cui viene mostrata la web app in azione, non è stata riportata nessuna demo del circuito poichè è praticamente lo stesso
del primo assignment e l'unica differenza consiste nel fatto che ora viene visualizzata anche la parte di weather sul display lcd (quando è attivo
sia home che weather il display alterna i contenuti).

Tabelle MySQL usate:
CREATE TABLE `gferrario`.`slave` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `name` VARCHAR(45) NOT NULL,
  `status` VARCHAR(45) NOT NULL,
  `time` TIMESTAMP NOT NULL DEFAULT 'CURRENT_TIMESTAMP',
  `mac` VARCHAR(45) NOT NULL,
  PRIMARY KEY (`id`));

use slave;

select * from slave;

Questa è usata per registrare le attività degli slave.
____________________________________________________________
CREATE TABLE `gferrario`.`slave_catalogue` (
  `name` VARCHAR(45) NOT NULL,
  `mac` VARCHAR(45) NOT NULL,
  `job` VARCHAR(45) NOT NULL,
  PRIMARY KEY (`name`, `mac`));


use slave_catalogue;

INSERT INTO `gferrario`.`slave_catalogue` (`name`, `mac`, `job`) VALUES ('NodeMCUHome', '40:F5:20:04:D3:00_home', 'home'), ('NodeMCUWeather', '40:F5:20:04:D3:00_weather', 'weather');

select * from slave_catalogue ;

Questa è usata per verificare l'identità degli slave che si vogliono connettere alla rete, quindi non permette intrusioni di dispositivi maligni nella rete.