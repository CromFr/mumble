/* Copyright (C) 2005-2010, Thorvald Natvig <thorvald@natvig.com>

   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   - Neither the name of the Mumble Developers nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "UserInformation.h"
#include "Net.h"
#include "ServerHandler.h"
#include "ViewCert.h"
#include "Audio.h"
#include "Global.h"

UserInformation::UserInformation(const MumbleProto::UserStats &msg, QWidget *p) : QDialog(p) {
	setupUi(this);
	
	uiSession = msg.session();
	
	qtTimer = new QTimer(this);
	connect(qtTimer, SIGNAL(timeout()), this, SLOT(tick()));
	qtTimer->start(12000);

	qgbConnection->setVisible(false);

	update(msg);
	resize(sizeHint());
	
	qfCertificateFont = qlCertificate->font();
}

unsigned int UserInformation::session() const {
	return uiSession;
}

void UserInformation::tick() {
	if (bRequested)
		return;
		
	bRequested = true;
	MumbleProto::UserStats mpus;
	mpus.set_session(uiSession);
	mpus.set_stats_only(true);
	g.sh->sendMessage(mpus);
}

void UserInformation::on_qpbCertificate_clicked() {
	ViewCert *vc = new ViewCert(qlCerts, this);
	vc->setAttribute(Qt::WA_DeleteOnClose, true);
	vc->show();
}

QString UserInformation::secsToString(unsigned int secs) {
	QStringList qsl;

	int weeks = secs / (60 * 60 * 24 * 7);
	secs = secs % (60 * 60 * 24 * 7);
	int days = secs / (60 * 60 * 24);
	secs = secs % (60 * 60 * 24);
	int hours = secs / (60 * 60);
	secs = secs % (60 * 60);
	int minutes = secs / 60;
	int seconds = secs % 60;
	
	if (weeks)
		qsl << tr("%1w").arg(weeks);
	if (days)
		qsl << tr("%1d").arg(days);
	if (hours)
		qsl << tr("%1h").arg(hours);
	if (minutes || hours)
		qsl << tr("%1m").arg(minutes);
	qsl << tr("%1s").arg(seconds);
	
	return qsl.join(tr(" "));
}

void UserInformation::update(const MumbleProto::UserStats &msg) {
	bRequested = false;
	
	bool showcon = false;
	
	if (msg.certificates_size() > 0) {
		showcon = true;
		qlCerts.clear();
		for(int i=0;i<msg.certificates_size(); ++i) {
			const std::string &s = msg.certificates(i);
			QList<QSslCertificate> certs = QSslCertificate::fromData(QByteArray(s.data(), s.length()), QSsl::Der);
			qlCerts <<certs;
		}
		if (! qlCerts.isEmpty()) {
			qpbCertificate->setEnabled(true);
			
			const QSslCertificate &cert = qlCerts.last();
			const QMultiMap<QSsl::AlternateNameEntryType, QString> &alts = cert.alternateSubjectNames();

			if (alts.contains(QSsl::EmailEntry))
				qlCertificate->setText(QStringList(alts.values(QSsl::EmailEntry)).join(tr(", ")));
			else	
				qlCertificate->setText(cert.subjectInfo(QSslCertificate::CommonName));
				
			if (msg.strong_certificate()) {
				QFont f = qfCertificateFont;
				f.setBold(true);
				qlCertificate->setFont(f);
			} else {
				qlCertificate->setFont(qfCertificateFont);
			}
		} else {
			qpbCertificate->setEnabled(false);
			qlCertificate->setText(QString());
		}
	}
	if (msg.has_address()) {
		showcon = true;
		HostAddress ha(msg.address());
		qlAddress->setText(ha.toString());
	}
	if (msg.has_version()) {
		showcon = true;

		const MumbleProto::Version &mpv = msg.version();
		unsigned int v = mpv.version();
		unsigned int major = (v >> 16) & 0xFFFF;
		unsigned int minor = (v >> 8) & 0xFF;
		unsigned int patch = (v & 0xFF);
		
		qlVersion->setText(tr("%1.%2.%3 (%4)").arg(major).arg(minor).arg(patch).arg(u8(mpv.release())));
		qlOS->setText(tr("%1 (%2)").arg(u8(mpv.os())).arg(u8(mpv.os_version())));
	}
	if (msg.celt_versions_size() > 0) {
		QStringList qsl;
		for(int i=0;i<msg.celt_versions_size(); ++i) {
			int v = msg.celt_versions(i);
			CELTCodec *cc = g.qmCodecs.value(v);
			if (cc) 
				qsl << cc->version();
			else
				qsl << QString::number(v, 16);
		}
		qlCELT->setText(qsl.join(tr(", ")));
	}
	if (showcon)
		qgbConnection->setVisible(true);
		
	qlTCPCount->setText(QString::number(msg.tcp_packets()));
	qlTCPAvg->setText(QString::number(msg.tcp_ping_avg(), 'f', 2));
	qlTCPVar->setText(QString::number(msg.tcp_ping_var(), 'f', 2));

	qlUDPCount->setText(QString::number(msg.udp_packets()));
	qlUDPAvg->setText(QString::number(msg.udp_ping_avg(), 'f', 2));
	qlUDPVar->setText(QString::number(msg.udp_ping_var(), 'f', 2));
	
	if (msg.has_from_client() && msg.has_from_server()) {
		qgbUDP->setVisible(true);
		const MumbleProto::UserStats_Stats &from = msg.from_client();
		qlFromGood->setText(QString::number(from.good()));
		qlFromLate->setText(QString::number(from.late()));
		qlFromLost->setText(QString::number(from.lost()));
		qlFromResync->setText(QString::number(from.resync()));

		const MumbleProto::UserStats_Stats &to = msg.from_server();
		qlToGood->setText(QString::number(to.good()));
		qlToLate->setText(QString::number(to.late()));
		qlToLost->setText(QString::number(to.lost()));
		qlToResync->setText(QString::number(to.resync()));
	} else {
		qgbUDP->setVisible(false);
	}
	
	if (msg.has_onlinesecs()) {
		if (msg.has_idlesecs())
			qlTime->setText(tr("%1 online (%2 idle)").arg(secsToString(msg.onlinesecs()), secsToString(msg.idlesecs())));
		else
			qlTime->setText(tr("%1 online").arg(secsToString(msg.onlinesecs())));
	} 
	if (msg.has_bandwidth()) {
		qlBandwidth->setVisible(true);
		qliBandwidth->setVisible(true);
		qlBandwidth->setText(tr("%1 kbit/s").arg(msg.bandwidth() / 125.0f, 0, 'f', 1));
	} else {
		qlBandwidth->setVisible(false);
		qliBandwidth->setVisible(false);
		qlBandwidth->setText(QString());
	}
}