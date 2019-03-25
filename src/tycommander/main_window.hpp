/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef MAIN_WINDOW_HH
#define MAIN_WINDOW_HH

#include <QHash>
#include <QProgressBar>
#include <QStringList>

#include <memory>
#include <vector>

#include "ui_main_window.h"

class AboutDialog;
class ArduinoDialog;
class Board;
class Monitor;

class MainWindow : public QMainWindow, private Ui::MainWindow {
    Q_OBJECT

    static QStringList codecs_;
    static QHash<QString, int> codec_indexes_;

    QMenu *menuUpload;
    QMenu *menuBrowseFirmware;
    QMenu *menuBoardContext;
    QMenu *menuEnableSerial;
    QAction *actionClearRecentFirmwares = nullptr;

#ifdef __APPLE__
    // See MainWindow::MainWindow() in main_window.cc for more information about that
    QMenu *menuRecentFirmwares2;
    QMenu *menuRecentFirmwares3;
#endif

    QMenu *menuSerialOptions;
    QAction *actionSerialEcho;
    QActionGroup *actionSerialEOLGroup;

    bool compact_mode_ = false;
    QComboBox *boardComboBox;
    // We need to keep this around to show/hide the board QComboBox
    QAction *actionBoardComboBox;
    QProgressBar *statusProgressBar;
    EnhancedGroupBox *lastOpenOptionBox = nullptr;
    int saved_splitter_pos_ = 1;

    Monitor *monitor_;
    std::vector<std::shared_ptr<Board>> selected_boards_;
    Board *current_board_ = nullptr;

    ArduinoDialog *arduino_dialog_ = nullptr;
    AboutDialog *about_dialog_ = nullptr;

public:
    MainWindow(QWidget *parent = nullptr);

    bool event(QEvent *ev) override;

    std::vector<std::shared_ptr<Board>> selectedBoards() const { return selected_boards_; }
    Board *currentBoard() const { return current_board_; }

    bool compactMode() const { return compact_mode_; }

public slots:
    void showErrorMessage(const QString &msg);

    void selectNextBoard();
    void selectPreviousBoard();

    void uploadToSelection();
    void uploadNewToSelection();
    void dropAssociationForSelection();
    void resetSelection();
    void rebootSelection();
    void sendToSelectedBoards(const QString &s);

    void setCompactMode(bool enable);

    void openCloneWindow();
    void openArduinoTool();
    void openPreferences();
    void openAboutDialog();

    void sendFileToSelection();
    void clearSerialDocument();

private:
    static void initCodecList();

    void fixEmptySelection(const QModelIndex &parent, int start, int end);
    void enableBoardWidgets();
    void disableBoardWidgets();
    void updateWindowTitle();
    void updateFirmwareMenus();
    void updateSerialLogLink();

    QString browseFirmwareDirectory() const;
    QString browseFirmwareFilter() const;

private slots:
    void selectionChanged(const QItemSelection &selected, const QItemSelection &previous);
    void openBoardListContextMenu(const QPoint &pos);

    void autoFocusBoardWidgets();

    void refreshActions();
    void refreshInfo();
    void refreshSettings();
    void refreshInterfaces();
    void refreshStatus();
    void refreshProgress();

    void openSerialContextMenu(const QPoint &pos);

    void validateAndSetFirmwarePath();
    void browseForFirmware();

    void setResetAfterForSelection(bool reset_after);
    void setSerialCodecForSelection(const QString &codec_name);
    void setClearOnResetForSelection(bool clear_on_reset);
    void setScrollBackLimitForSelection(int limit);
    void setEnableSerialForSelection(bool enable);
    void setSerialLogSizeForSelection(int size);
};

#endif
