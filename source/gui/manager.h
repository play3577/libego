#ifndef MANAGER_H_
#define MANAGER_H_

#include <QObject>
#include <QString>
#include <QDialog>
#include <QLabel>


class GameScene;
class Engine;
class Move;
class Player;

class Manager : public QDialog {
public:
  Manager (Engine& engine);

  virtual ~Manager ();

  void refreshBoard ();

public slots:
  void handleMousePress (int x, int y, Qt::MouseButtons buttons);
  void handleHooverEntered (int x, int y);
  void setKomi (double);
  void playMove ();
  void undoMove ();
  void showGammas (int);
  void setStatus (QString);

signals:
  void stateChanged (const Player&);
  void statusChanged (QString);

private:
  QLabel* statebar;
  GameScene* game_scene;
  Engine& engine_;

private:
  Q_OBJECT
};

#endif /* MANAGER_H_ */
