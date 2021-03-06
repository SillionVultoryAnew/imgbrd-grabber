#ifndef HEADER_QAFFICHE
#define HEADER_QAFFICHE

#include <QLabel>
#include <QVariant>



class QAffiche : public QLabel
{
	Q_OBJECT
	
	public:
		QAffiche(QVariant id = QVariant(), int border = 0, QColor color = QColor(), QWidget *parent = 0);
		~QAffiche();
		void setImage(QImage);
		void setImage(QPixmap);
		Qt::MouseButton lastPressed();
	
	signals:
		void doubleClicked();
		void doubleClicked(int);
		void clicked();
		void clicked(int);
		void clicked(QString);
		void middleClicked();
		void middleClicked(int);
		void middleClicked(QString);
		void pressed();
		void pressed(int);
		void released();
		void released(int);
		void mouseOver();
		void mouseOver(int);
		void mouseOut();
		void mouseOut(int);
		
	protected:
		//void paintEvent(QPaintEvent*);
		void mouseDoubleClickEvent(QMouseEvent*);
		void mousePressEvent(QMouseEvent*);
		void mouseReleaseEvent(QMouseEvent*);
		void enterEvent(QEvent*);
		void leaveEvent(QEvent*);
		bool hitLabel(const QPoint &p);
		bool m_pressed;
		QVariant m_id;
		int m_border;
		QColor m_color;
		Qt::MouseButton m_lastPressed;
};

#endif
