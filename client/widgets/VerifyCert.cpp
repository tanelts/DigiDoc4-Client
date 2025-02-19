/*
 * QDigiDoc4
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "VerifyCert.h"
#include "ui_VerifyCert.h"
#include "Styles.h"

#include <common/DateTime.h>
#include <common/SslCertificate.h>

#include <QtCore/QTextStream>
#include <QtNetwork/QSslKey>
#include <QSvgWidget>

VerifyCert::VerifyCert(QWidget *parent): StyledWidget(parent), ui(new Ui::VerifyCert)
{
	ui->setupUi( this );

	connect( ui->changePIN, &QPushButton::clicked, this, &VerifyCert::processClickedBtn );
	connect( ui->forgotPinLink, &QLabel::linkActivated, this, &VerifyCert::processForgotPinLink );
	connect( ui->details, &QLabel::linkActivated, this, &VerifyCert::processCertDetails );

	greenIcon = new QSvgWidget(QStringLiteral(":/images/icon_check.svg"), ui->nameRight);
	greenIcon->resize(12, 13);
	greenIcon->move(5, 5);
	greenIcon->hide();
	greenIcon->setStyleSheet(QStringLiteral("border: none;"));

	orangeIcon = new QSvgWidget(QStringLiteral(":/images/icon_alert_orange.svg"), ui->nameRight);
	orangeIcon->resize(13, 13);
	orangeIcon->move(5, 5);
	orangeIcon->hide();
	orangeIcon->setStyleSheet(QStringLiteral("border: none;"));

	redIcon = new QSvgWidget(QStringLiteral(":/images/icon_alert_red.svg"), ui->nameRight);
	redIcon->resize(13, 13);
	redIcon->move(5, 5);
	redIcon->hide();
	redIcon->setStyleSheet(QStringLiteral("border: none;"));

	ui->name->setFont( Styles::font( Styles::Regular, 18, QFont::Bold  ) );
	ui->validUntil->setFont( Styles::font( Styles::Regular, 14 ) );
	ui->error->setFont( Styles::font( Styles::Regular, 12, QFont::DemiBold ) );
	QFont regular12 = Styles::font( Styles::Regular, 12 );
	ui->forgotPinLink->setFont( regular12 );
	ui->details->setFont( regular12 );
	ui->changePIN->setFont( Styles::font( Styles::Condensed, 14 ) );
	ui->tempelText->setFont( Styles::font( Styles::Regular, 14 ) );
	ui->tempelText->hide();
	borders = QStringLiteral(" border: solid #DFE5E9; border-width: 1px 0px 0px 0px; ");
}

VerifyCert::~VerifyCert()
{
	delete ui;
}

void VerifyCert::addBorders()
{
	// Add top, right and left border shadows
	borders = QStringLiteral(" border: solid #DFE5E9; border-width: 1px 1px 0px 1px; ");
}

void VerifyCert::clear()
{
	c = SslCertificate();
	isBlockedPin = false;
	cardData = QSmartCardData();
	update();
}

void VerifyCert::update( QSmartCardData::PinType type, const QSmartCard *pSmartCard )
{
	this->pinType = type;
	this->cardData = pSmartCard->data();
	c = (pinType == QSmartCardData::Pin1Type) ? cardData.authCert() : cardData.signCert();
	isBlockedPin = cardData.retryCount( pinType ) == 0;
	update();
}

void VerifyCert::update(QSmartCardData::PinType type, const SslCertificate &cert)
{
	pinType = type;
	c = cert;
	isBlockedPin = false;
	
	update();
}

void VerifyCert::update(bool showWarning)
{
	bool isBlockedPuk = !cardData.isNull() && (cardData.retryCount( QSmartCardData::PukType ) == 0);
	isTempelType = c.type() & SslCertificate::TempelType;
	bool isRevoked = pinType != QSmartCardData::PukType &&
			cardData.version() >= QSmartCardData::VER_3_5 &&
			cardData.version() < QSmartCardData::VER_IDEMIA &&
			cardData.authCert().publicKey().algorithm() == QSsl::Rsa;
	isValidCert = c.isNull() || (c.isValid() && !isRevoked);

	QString name;
	QString changeBtn;
	QString forgotPinText;
	QString detailsText;
	QString error;
	QString txt;
	QTextStream cert( &txt );

	ui->changePIN->show();

	if(isRevoked)
		cert << tr("Certificate is revoked!");
	else if( !isValidCert )
		cert << tr("Certificate has expired!");
	else
	{
		qint64 leftDays = std::max<qint64>(0, QDateTime::currentDateTime().daysTo(c.expiryDate().toLocalTime()));
		if(leftDays <= 105 && !c.isNull())
			cert << "<span style='color: #F0BF72'>";
		cert << tr("Certificate%1is valid%2 until %3").arg(
				QStringLiteral(" <span style='color: #37a447'>"),
				QStringLiteral("</span> "),
				DateTime(c.expiryDate().toLocalTime()).formatDate(QStringLiteral("dd. MMMM yyyy")));
		if(leftDays <= 105 && !c.isNull())
			cert << "</span>";
		if(!isTempelType && cardData.version() != QSmartCardData::VER_IDEMIA)
		{
			if(pinType == QSmartCardData::Pin1Type)
				cert << "<br />" << tr("key has been used %1 times", "pin1").arg(cardData.usageCount(pinType));
			else
				cert << "<br />" << tr("key has been used %1 times", "pin2").arg(cardData.usageCount(pinType));
		}
	}
	switch( pinType )
	{
	case QSmartCardData::Pin1Type:
		name = tr("Authentication certificate");
		changeBtn = isBlockedPin ? tr("UNBLOCK") : tr("CHANGE PIN1");
		if(isTempelType || (!cardData.isNull() && cardData.isSecurePinpad()) || !isValidCert)
			forgotPinText.clear();
		else
			forgotPinText = tr("%1Forgot PIN%2?%3")
							.arg(QStringLiteral("<a href='#pin1-forgotten'><span style='color:#75787B;'>"))
							.arg(1)
							.arg(QStringLiteral("</span></a>"));
		detailsText = tr("%1Check the details of the certificate%2").arg(QStringLiteral("<a href='#pin1-cert'><span style='color:#75787B;'>"), QStringLiteral("</span></a>"));
		error = ( isRevoked ) ? tr("PIN%1 can not be used because the certificate has revoked. You can find instructions on how to get a new document from <a href=\"https://www.politsei.ee/en/\"><span style=\"color: #006EB5; text-decoration: none;\">here</span></a>.").arg(1) :
				( !isValidCert ) ? tr("PIN%1 can not be used because the certificate has expired. Update certificate to reuse PIN%1.").arg(1) :
				( isBlockedPin ) ? tr("PIN%1 has been blocked because PIN%1 code has been entered incorrectly 3 times. Unblock to reuse PIN%1.").arg(1) :
				QString();
		break;
	case QSmartCardData::Pin2Type:
		name = tr("Signing certificate");
		changeBtn = isBlockedPin ? tr("UNBLOCK") : tr("CHANGE PIN2");
		if(isTempelType || (!cardData.isNull() && cardData.isSecurePinpad()) || !isValidCert)
			forgotPinText.clear();
		else
			forgotPinText = tr("%1Forgot PIN%2?%3")
							.arg(QStringLiteral("<a href='#pin1-forgotten'><span style='color:#75787B;'>"))
							.arg(2)
							.arg(QStringLiteral("</span></a>"));
		detailsText = tr("%1Check the details of the certificate%2").arg(QStringLiteral("<a href='#pin1-cert'><span style='color:#75787B;'>"), QStringLiteral("</span></a>"));
		error = ( isRevoked ) ? tr("PIN%1 can not be used because the certificate has revoked. You can find instructions on how to get a new document from <a href=\"https://www.politsei.ee/en/\"><span style=\"color: #006EB5; text-decoration: none;\">here</span></a>.").arg(2) :
				( !isValidCert ) ? tr("PIN%1 can not be used because the certificate has expired. Update certificate to reuse PIN%1.").arg(2) :
				( isBlockedPin ) ? tr("PIN%1 has been blocked because PIN%1 code has been entered incorrectly 3 times. Unblock to reuse PIN%1.").arg(2) :
				QString();
		break;
	case QSmartCardData::PukType:
		name = tr("PUK code");
		txt = tr("The PUK code is located in your envelope");
		changeBtn = tr("CHANGE PUK");
		error = ( isBlockedPin ) ?
			tr("PUK code is blocked because the PUK code has been entered 3 times incorrectly. "
				"You can not unblock the PUK code yourself. As long as the PUK code is blocked, all eID options can be used, except PUK code. "
				"Please visit the service center to obtain new codes. <a href=\"http://www.politsei.ee/et/nouanded/id-kaart-ja-pass/kui-id-kaardi-koodid-kaovad/\">Additional information</a>.")
			: QString();
		ui->changePIN->setDisabled(cardData.version() == QSmartCardData::VER_USABLEUPDATER);
		break;
	default:
		break;
	}



	if( !isValidCert && pinType != QSmartCardData::PukType )
	{
		this->setStyleSheet( "opacity: 0.25; background-color: #F9EBEB;"  + borders );
		ui->verticalSpacerAboveBtn->changeSize( 20, 8 );
		ui->verticalSpacerBelowBtn->changeSize( 20, 6 );
		ui->changePIN->setStyleSheet(
					"QPushButton { border-radius: 2px; border: none; color: #ffffff; background-color: #006EB5;}"
					"QPushButton:pressed { background-color: #41B6E6;}"
					"QPushButton:hover:!pressed { background-color: #008DCF;}" 
					"QPushButton:disabled { background-color: #BEDBED;};" 
					); 
		ui->error->setStyleSheet(
					"padding: 6px 6px 6px 6px;"
					"line-height: 14px;"
					"border: 1px solid #c53e3e;"
					"border-radius: 2px;"
					"background-color: #e09797;"
					"color: #5c1c1c;"
					);
		ui->name->setText(name);
		greenIcon->hide();
		orangeIcon->hide();
		redIcon->show();
	}
	else if( isBlockedPin )
	{
		this->setStyleSheet( "opacity: 0.25; background-color: #fcf5ea;"  + borders );
		ui->verticalSpacerAboveBtn->changeSize( 20, 8 );
		ui->verticalSpacerBelowBtn->changeSize( 20, 6 );
		ui->changePIN->setStyleSheet(
					"QPushButton { border-radius: 2px; border: none; color: #ffffff; background-color: #006EB5;}" 
					"QPushButton:pressed { background-color: #41B6E6;}" 
					"QPushButton:hover:!pressed { background-color: #008DCF;}" 
					"QPushButton:disabled { background-color: #BEDBED;};" 
					); 
		ui->error->setStyleSheet(
					"padding: 6px 6px 6px 6px;"
					"line-height: 14px;"
					"border: 1px solid #e89c30;"
					"border-radius: 2px;"
					"background-color: #F8DDA7;"
					);
		ui->name->setText(name);
		greenIcon->hide();
		redIcon->hide();
		orangeIcon->show();
	}
	else
	{
		this->setStyleSheet( "background-color: #ffffff;" + borders );
		// Check height: if warning shown, decrease height by 30px (15*2)
		int decrease = height() < 210 ? 15 : 0;
		ui->verticalSpacerAboveBtn->changeSize(20, 29 - decrease);
		ui->verticalSpacerBelowBtn->changeSize(20, 34 - decrease);
		changePinStyle(QStringLiteral("#FFFFFF"));
		ui->name->setText(name);
		redIcon->hide();
		orangeIcon->setVisible(showWarning && pinType != QSmartCardData::PukType);
		greenIcon->setVisible(!showWarning && pinType != QSmartCardData::PukType);
	}
	ui->name->setTextFormat( Qt::RichText );

	ui->forgotPinLink->setText( forgotPinText );
	ui->forgotPinLink->setVisible( !forgotPinText.isEmpty() );

	ui->details->setText( detailsText );
	ui->details->setVisible( !detailsText.isEmpty() );
	
	if( isBlockedPuk )
	{
		if( pinType != QSmartCardData::PukType )
		{
			ui->forgotPinLink->setVisible( false ); // hide 'Forgot PIN. code?' label
			ui->changePIN->setHidden(isBlockedPin);  // hide 'Tühista BLOKEERING' button
		}
		else
		{
			ui->widget->setMinimumHeight ( 0 );
			ui->verticalSpacerBelowBtn->changeSize( 20, 0 );
			ui->validUntil->setVisible( false );
			ui->changePIN->hide();  // hide 'change PUK' button
		}
	}
	else
		ui->changePIN->setVisible(isValidCert);  // If not valid cert then hide 'CHANGE PIN/PUK' button
	ui->validUntil->setText( txt );
	if( pinType != QSmartCardData::PukType )
		ui->error->setVisible( isBlockedPin || !isValidCert );
	else
		ui->error->setVisible( isBlockedPin );
	ui->error->setText( error );
	ui->tempelText->setVisible(isTempelType);
	if(isTempelType)
		ui->changePIN->hide();
	ui->changePIN->setText(changeBtn);
	ui->changePIN->setAccessibleName(changeBtn.toLower());
}

void VerifyCert::enterEvent(QEvent * /*event*/)
{
	if( !isValidCert && pinType != QSmartCardData::PukType )
		setStyleSheet("background-color: #f3d8d8;" + borders);
	else if( isBlockedPin )
		setStyleSheet("background-color: #fbecd0;" + borders);
	else
	{
		setStyleSheet("background-color: #f7f7f7;" + borders);
		changePinStyle(QStringLiteral("#f7f7f7"));
	}
}

void VerifyCert::leaveEvent(QEvent * /*event*/)
{
	if( !isValidCert && pinType != QSmartCardData::PukType )
		setStyleSheet("background-color: #F9EBEB;" + borders);
	else if( isBlockedPin )
		setStyleSheet("background-color: #fcf5ea;" + borders);
	else
	{
		setStyleSheet( "background-color: white;" + borders);
		changePinStyle(QStringLiteral("white"));
	}
}

void VerifyCert::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		update(orangeIcon->isVisible());
	}

	QWidget::changeEvent(event);
}


void VerifyCert::changePinStyle( const QString &background )  
{
	ui->changePIN->setStyleSheet(QStringLiteral(
		"QPushButton { border-radius: 2px; border: 1px solid #006EB5; color: #006EB5; background-color: %1;}"
		"QPushButton:pressed { border: none; background-color: #006EB5; color: %1;}"
		"QPushButton:hover:!pressed { border-radius: 2px; border: 1px solid #008DCF; color: #008DCF;}"
		"QPushButton:disabled { border: 1px solid #BEDBED; color: #BEDBED;};").arg( background )
	);
}

void VerifyCert::processClickedBtn()
{
	if(!c.isNull())
		emit changePinClicked( false, isBlockedPin );
}

void VerifyCert::processForgotPinLink(const QString & /*link*/)
{
	if(!c.isNull())
		emit changePinClicked( true, false );	// Change PIN with PUK code
}

void VerifyCert::processCertDetails(const QString & /*link*/)
{
	if(!c.isNull())
		emit certDetailsClicked(QSmartCardData::typeString(pinType));
}

void VerifyCert::showWarningIcon()
{
	if(redIcon->isVisible())
		return;

	greenIcon->hide();
	orangeIcon->show();
	redIcon->hide();
}
