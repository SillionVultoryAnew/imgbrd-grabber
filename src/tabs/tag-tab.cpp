#include <QMessageBox>
#include "tag-tab.h"
#include "ui_tag-tab.h"
#include "ui_mainwindow.h"
#include "ui/QBouton.h"
#include "viewer/zoomwindow.h"
#include "searchwindow.h"

extern mainWindow *_mainwindow;



tagTab::tagTab(int id, QMap<QString,Site*> *sites, QList<Favorite> favorites, mainWindow *parent)
	: searchTab(id, sites, parent), ui(new Ui::tagTab), m_id(id), m_parent(parent), m_favorites(favorites), m_pagemax(-1), m_lastTags(QString()), m_sized(false), m_from_history(false), m_stop(true), m_history_cursor(0), m_history(QList<QMap<QString,QString> >()), m_modifiers(QStringList())
{
	ui->setupUi(this);
	ui->widgetMeant->hide();
	setAttribute(Qt::WA_DeleteOnClose);

	QSettings settings(savePath("settings.ini"), QSettings::IniFormat, this);
	m_ignored = loadIgnored();

	// Search fields
	QStringList favs;
	for (Favorite fav : m_favorites)
		favs.append(fav.getName());
	m_search = new TextEdit(favs, this);
	m_postFiltering = new TextEdit(favs, this);
		m_search->setContextMenuPolicy(Qt::CustomContextMenu);
		m_postFiltering->setContextMenuPolicy(Qt::CustomContextMenu);
		if (settings.value("autocompletion", true).toBool())
		{
			QFile words("words.txt");
			if (words.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				while (!words.atEnd())
					m_completion.append(QString(words.readLine()).trimmed().split(" ", QString::SkipEmptyParts));
				words.close();
			}
			QFile wordsc(savePath("wordsc.txt"));
			if (wordsc.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				while (!wordsc.atEnd())
					m_completion.append(QString(wordsc.readLine()).trimmed().split(" ", QString::SkipEmptyParts));
				wordsc.close();
			}
			for (int i = 0; i < sites->size(); i++)
				if (sites->value(sites->keys().at(i))->contains("Modifiers"))
					m_modifiers.append(sites->value(sites->keys().at(i))->value("Modifiers").trimmed().split(" ", QString::SkipEmptyParts));
			m_completion.append(m_modifiers);
			m_completion.append(favs);
			m_completion.removeDuplicates();
			m_completion.sort();
			QCompleter *completer = new QCompleter(m_completion, this);
				completer->setCaseSensitivity(Qt::CaseInsensitive);
			m_search->setCompleter(completer);
			m_postFiltering->setCompleter(completer);
		}
		connect(m_search, SIGNAL(returnPressed()), this, SLOT(load()));
		connect(m_search, SIGNAL(favoritesChanged()), _mainwindow, SLOT(updateFavorites()));
		connect(m_search, SIGNAL(favoritesChanged()), _mainwindow, SLOT(updateFavoritesDock()));
		connect(m_search, SIGNAL(kflChanged()), _mainwindow, SLOT(updateKeepForLater()));
		connect(m_postFiltering, SIGNAL(returnPressed()), this, SLOT(load()));
		connect(ui->labelMeant, SIGNAL(linkActivated(QString)), this, SLOT(setTags(QString)));
		ui->layoutFields->insertWidget(1, m_search, 1);
		ui->layoutPlus->addWidget(m_postFiltering, 1, 1, 1, 3);

	// Sources
	QString sel = '1'+QString().fill('0',m_sites->count()-1);
	QString sav = settings.value("sites", sel).toString();
	for (int i = 0; i < sel.count(); i++)
	{
		if (sav.count() <= i)
		{ sav[i] = '0'; }
		m_selectedSources.append(sav.at(i) == '1' ? true : false);
	}

	// Others
	ui->checkMergeResults->setChecked(settings.value("mergeresults", false).toBool());
	optionsChanged();
	ui->widgetPlus->hide();
	setWindowIcon(QIcon());
	updateCheckboxes();
	m_search->setFocus();
}

tagTab::~tagTab()
{
	close();
	delete ui;
}

void tagTab::updateCheckboxes()
{
	log(tr("Mise à jour des cases à cocher."));
	qDeleteAll(m_checkboxes);
	m_checkboxes.clear();
	QStringList urls = m_sites->keys();
	QSettings settings(savePath("settings.ini"), QSettings::IniFormat, this);
	int n = settings.value("Sources/Letters", 3).toInt(), m = n;
	for (int i = 0; i < urls.size(); i++)
	{
		if (urls[i].startsWith("www."))
		{ urls[i] = urls[i].right(urls[i].length() - 4); }
		else if (urls[i].startsWith("chan."))
		{ urls[i] = urls[i].right(urls[i].length() - 5); }
		if (n < 0)
		{
			m = urls.at(i).indexOf('.');
			if (n < -1 && urls.at(i).indexOf('.', m+1) != -1)
			{ m = urls.at(i).indexOf('.', m+1); }
		}

		bool isChecked = m_selectedSources.size() > i ? m_selectedSources.at(i) : false;
		QCheckBox *c = new QCheckBox(urls.at(i).left(m), this);
			c->setChecked(isChecked);
			ui->layoutSourcesList->addWidget(c);

		m_checkboxes.append(c);
	}
	DONE();
}

void tagTab::on_buttonSearch_clicked()
{
	SearchWindow *sw = new SearchWindow(m_search->toPlainText(), this);
	connect(sw, SIGNAL(accepted(QString)), this, SLOT(setTags(QString)));
	sw->show();
}

void tagTab::closeEvent(QCloseEvent *e)
{
	QSettings settings(savePath("settings.ini"), QSettings::IniFormat, this);
	settings.setValue("mergeresults", ui->checkMergeResults->isChecked());
	settings.sync();

	qDeleteAll(m_pages);
	m_pages.clear();
	m_images.clear();
	qDeleteAll(m_checkboxes);
	m_checkboxes.clear();
	for (int i = 0; i < m_layouts.size(); i++)
	{ clearLayout(m_layouts[i]); }
	qDeleteAll(m_layouts);
	m_layouts.clear();
	m_boutons.clear();

	emit closed(this);
	e->accept();
}



void tagTab::optionsChanged()
{
	log(tr("Mise à jour des options de l'onglet \"%1\".").arg(windowTitle()));
	QSettings settings(savePath("settings.ini"), QSettings::IniFormat, this);
	ui->retranslateUi(this);
	ui->spinImagesPerPage->setValue(settings.value("limit", 20).toInt());
	ui->spinColumns->setValue(settings.value("columns", 1).toInt());
	/*QPalette p = ui->widgetResults->palette();
	p.setColor(ui->widgetResults->backgroundRole(), QColor(settings.value("serverBorderColor", "#000000").toString()));
	ui->widgetResults->setPalette(p);*/
	ui->layoutResults->setHorizontalSpacing(settings.value("Margins/main", 10).toInt());
}



void tagTab::load()
{
	log(tr("Chargement des résultats..."));

	ui->buttonGetAll->setEnabled(false);
	ui->buttonGetpage->setEnabled(false);
	ui->buttonGetSel->setEnabled(false);

	m_stop = true;
	m_parent->ui->labelWiki->setText("");
	m_pagemax = -1;

	if (!m_from_history)
	{
		QMap<QString,QString> srch = QMap<QString,QString>();
		srch["tags"] = m_search->toPlainText();
		srch["page"] = QString::number(ui->spinPage->value());
		srch["ipp"] = QString::number(ui->spinImagesPerPage->value());
		srch["columns"] = QString::number(ui->spinColumns->value());
		m_history.append(srch);

		if (m_history.size() > 1)
		{
			m_history_cursor++;
			ui->buttonHistoryBack->setEnabled(true);
			ui->buttonHistoryNext->setEnabled(false);
		}
	}
	m_from_history = false;

	if (m_search->toPlainText() != m_lastTags && !m_lastTags.isNull() && m_history_cursor == m_history.size() - 1)
	{ ui->spinPage->setValue(1); }
	m_lastTags = m_search->toPlainText();

	ui->widgetMeant->hide();
	ui->buttonFirstPage->setEnabled(ui->spinPage->value() > 1);
	ui->buttonPreviousPage->setEnabled(ui->spinPage->value() > 1);

	// Clear results layout
	for (int i = 0; i < m_layouts.size(); i++)
	{ clearLayout(m_layouts[i]); }
	qDeleteAll(m_layouts);
	m_layouts.clear();
	m_boutons.clear();
	clearLayout(ui->layoutResults);
	ui->verticalLayout->removeWidget(ui->widgetResults);
	ui->widgetResults->deleteLater();
	ui->widgetResults = new QWidget(this);
	ui->layoutResults = new QGridLayout(ui->widgetResults);
	ui->verticalLayout->insertWidget(0, ui->widgetResults);

	setWindowTitle(m_search->toPlainText().isEmpty() ? tr("Recherche") : m_search->toPlainText().replace("&", "&&"));
	emit titleChanged(this);
	m_tags.clear();
	m_parent->setTags(m_tags, this);
	for (int i = 0; i < m_pages.size(); i++)
	{
		m_pages.value(m_pages.keys().at(i))->abort();
		m_pages.value(m_pages.keys().at(i))->abortTags();
	}
	//qDeleteAll(m_pages);
	m_pages.clear();
	for (int i = 0; i < m_images.size(); i++)
	{ m_images.at(i)->abortPreview(); }
	//qDeleteAll(m_images);
	m_images.clear();

	QSettings settings(savePath("settings.ini"), QSettings::IniFormat, this);
	for (int i = 0; i < m_selectedSources.size(); i++)
	{
		if (m_checkboxes.at(i)->isChecked())
		{
            // Get the search values
			QStringList tags = m_search->toPlainText().trimmed().split(" ", QString::SkipEmptyParts);
			tags.append(settings.value("add").toString().trimmed().split(" ", QString::SkipEmptyParts));
			int perpage = ui->spinImagesPerPage->value();

            // Load results
			Page *page = new Page(m_sites->value(m_sites->keys().at(i)), m_sites, tags, ui->spinPage->value(), perpage, m_postFiltering->toPlainText().split(" ", QString::SkipEmptyParts), false, this, 0, m_lastPage, m_lastPageMinId, m_lastPageMaxId);
			log(tr("Chargement de la page <a href=\"%1\">%1</a>").arg(page->url().toString().toHtmlEscaped()));
			connect(page, SIGNAL(finishedLoading(Page*)), this, SLOT(finishedLoading(Page*)));
			connect(page, SIGNAL(failedLoading(Page*)), this, SLOT(failedLoading(Page*)));
			m_pages.insert(page->website(), page);

            // Setup the layout
            QGridLayout *l = new QGridLayout;
			l->setHorizontalSpacing(settings.value("Margins/horizontal", 6).toInt());
			l->setVerticalSpacing(settings.value("Margins/vertical", 6).toInt());
			m_layouts.append(l);

            // Load tags if necessary
			m_stop = false;
			if (settings.value("useregexfortags", true).toBool())
			{
				connect(page, SIGNAL(finishedLoadingTags(Page*)), this, SLOT(finishedLoadingTags(Page*)));
				page->loadTags();
			}

            // Start loading
			page->load();
		}
	}
	if (ui->checkMergeResults->isChecked() && m_layouts.size() > 0)
	{ ui->layoutResults->addLayout(m_layouts[0], 0, 0); }
	m_page = 0;

	emit changed(this);
}

void tagTab::finishedLoading(Page* page)
{
	if (m_stop)
		return;

	log(tr("Réception de la page <a href=\"%1\">%1</a>").arg(page->url().toString().toHtmlEscaped()));
	QSettings settings(savePath("settings.ini"), QSettings::IniFormat, this);

	m_lastPage = page->page();
	m_lastPageMinId = page->minId();
	m_lastPageMaxId = page->maxId();
	QList<Image*> imgs = page->images();
	m_images.append(imgs);

	int maxpage = page->pagesCount();
	if (maxpage < m_pagemax || m_pagemax == -1)
		m_pagemax = maxpage;
	ui->buttonNextPage->setEnabled(maxpage > ui->spinPage->value() || page->imagesCount() == -1 || (page->imagesCount() == 0 && page->images().count() > 0));
	ui->buttonLastPage->setEnabled(maxpage > ui->spinPage->value());

	if (!ui->checkMergeResults->isChecked())
	{
		int pos = m_pages.values().indexOf(page);
		if (pos < 0)
			return;
		QLabel *txt = new QLabel(this);
		m_labels.append(txt);
			if (imgs.count() == 0)
			{
				QStringList reasons = QStringList();
				if (page->source().isEmpty())
				{ reasons.append(tr("serveur hors-ligne")); }
				if (m_search->toPlainText().count(" ") > 1)
				{ reasons.append(tr("trop de tags")); }
				if (ui->spinPage->value() > 1000)
				{ reasons.append(tr("page trop éloignée")); }

				// Maybe you meant
				QStringList completion;
				QFile words("words.txt");
				if (words.open(QIODevice::ReadOnly | QIODevice::Text) && !m_search->toPlainText().isEmpty())
				{
					while (!words.atEnd())
					{
						QByteArray line = words.readLine();
						completion.append(QString(line).remove("\r\n").remove("\n").split(" ", QString::SkipEmptyParts));
					}
					QStringList favs;
					for (Favorite fav : m_favorites)
						favs.append(fav.getName());
					completion.append(favs);
					completion.removeDuplicates();
					completion.sort();
					QStringList tags = m_search->toPlainText().trimmed().split(" ");
					QMap<QString,QString> results, clean;
					int c = 0;
					for (int t = 0; t < tags.size(); t++)
					{
						QString tag = tags.at(t);
						int lev = (tag.length()/3)+2;
						for (int w = 0; w < completion.size(); w++)
						{
							int d = levenshtein(tag, completion.at(w));
							if (d < lev)
							{
								if (results[tag].isEmpty())
								{ c++; }
								results[tag] = "<b>"+completion.at(w)+"</b>";
								clean[tag] = completion.at(w);
								lev = d;
							}
						}
						if (lev == 0)
						{
							results[tag] = tag;
							c--;
						}
					}
					if (c > 0)
					{
						QStringList res = results.values(), cl = clean.values();
						ui->widgetMeant->show();
						ui->labelMeant->setText("<a href=\""+cl.join(" ").toHtmlEscaped()+"\" style=\"color:black;text-decoration:none;\">"+res.join(" ")+"</a>");
					}
					words.close();
				}
				txt->setText("<a href=\""+page->url().toString().toHtmlEscaped()+"\">"+page->site()->name()+"</a> - "+tr("Aucun résultat")+(reasons.count() > 0 ? "<br/>"+tr("Raisons possibles : %1").arg(reasons.join(", ")) : ""));
			}
			else
			{ txt->setText("<a href=\""+page->url().toString().toHtmlEscaped()+"\">"+page->site()->name()+"</a> - "+tr("Page %1 sur %2 (%3 sur %4)").arg(ui->spinPage->value()).arg(page->pagesCount(false) > 0 ? QString::number(maxpage) : "?").arg(imgs.count()).arg(page->imagesCount(false) > 0 ? QString::number(page->imagesCount(false)) : "?")); }
			txt->setOpenExternalLinks(true);
			if (page->search().join(" ") != m_search->toPlainText() && settings.value("showtagwarning", true).toBool())
			{
				QStringList uncommon = m_search->toPlainText().trimmed().split(" ", QString::SkipEmptyParts);
				uncommon.append(settings.value("add").toString().trimmed().split(" ", QString::SkipEmptyParts));
				for (int i = 0; i < page->search().size(); i++)
				{
					if (uncommon.contains(page->search().at(i)))
					{ uncommon.removeAll(page->search().at(i)); }
				}
				if (!uncommon.isEmpty())
				{ txt->setText(txt->text()+"<br/>"+QString(tr("Des modificateurs ont été otés de la recherche car ils ne sont pas compatibles avec cet imageboard : %1.")).arg(uncommon.join(" "))); }
			}
			if (!page->errors().isEmpty() && settings.value("showwarnings", true).toBool())
			{ txt->setText(txt->text()+"<br/>"+page->errors().join("<br/>")); }

		int page_x = pos % ui->spinColumns->value();
		int page_y = (pos / ui->spinColumns->value()) * 2;
		ui->layoutResults->addWidget(txt, page_y, page_x);
		ui->layoutResults->setRowMinimumHeight(page_y, height() / 20);
		if (m_layouts.size() > pos)
		{ ui->layoutResults->addLayout(m_layouts[pos], page_y + 1, page_x); }
	}

	if (!settings.value("useregexfortags", true).toBool())
	{
		// Tags for this page
		QList<Tag> taglist;
		QStringList tagsGot;
		for (int i = 0; i < m_pages.count(); i++)
		{
			QList<Tag> tags = m_pages.value(m_pages.keys().at(i))->tags();
			for (int t = 0; t < tags.count(); t++)
			{
				if (!tags[t].text().isEmpty())
				{
					if (tagsGot.contains(tags[t].text()))
					{ taglist[tagsGot.indexOf(tags[t].text())].setCount(taglist[tagsGot.indexOf(tags[t].text())].count()+tags[t].count()); }
					else
					{
						taglist.append(tags[t]);
						tagsGot.append(tags[t].text());
					}
				}
			}
		}

		// We sort tags by frequency
		qSort(taglist.begin(), taglist.end(), sortByFrequency);

		m_tags = taglist;
		m_parent->setTags(m_tags, this);
	}

	postLoading(page);
}

void tagTab::failedLoading(Page *page)
{
	if (ui->checkMergeResults->isChecked())
	{
		postLoading(page);
	}
}

void tagTab::postLoading(Page *page)
{
	QSettings settings(savePath("settings.ini"), QSettings::IniFormat, this);
	QList<Image*> imgs;

	m_page++;
	if (ui->checkMergeResults->isChecked())
	{
		if (m_page != m_pages.size())
			return;

		QStringList md5s;
		for (int i = 0; i < m_images.count(); i++)
		{
			QString md5 = m_images.at(i)->md5();
			if (md5.isEmpty())
				continue;

			if (md5s.contains(md5))
				m_images.removeAt(i--);
			else
				md5s.append(md5);
		}

		imgs = m_images;
	}
	else
	{ imgs = page->images(); }

	// Loading images thumbnails
	for (int i = 0; i < imgs.count(); i++)
	{
		QStringList detected;
		Image *img = imgs.at(i);
		QStringList tags = m_search->toPlainText().toLower().split(' ');
		QList<QChar> modifiers = QList<QChar>() << '~';
		for (int r = 0; r < tags.size(); r++)
		{
			if (modifiers.contains(tags[r][0]))
			{ tags[r] = tags[r].right(tags[r].size()-1); }
		}
		if (!settings.value("blacklistedtags").toString().isEmpty())
		{ detected = img->blacklisted(settings.value("blacklistedtags").toString().toLower().split(" ")); }
		if (!detected.isEmpty() && settings.value("hideblacklisted", false).toBool())
		{ log(tr("Image #%1 ignorée. Raison : %2.").arg(i).arg("\""+detected.join(", ")+"\""));; }
		else
		{
			connect(img, SIGNAL(finishedLoadingPreview(Image*)), this, SLOT(finishedLoadingPreview(Image*)));
			img->loadPreview();
		}
	}

	ui->buttonGetAll->setDisabled(m_images.empty());
	ui->buttonGetpage->setDisabled(m_images.empty());
	ui->buttonGetSel->setDisabled(m_images.empty());
}


void tagTab::finishedLoadingTags(Page *page)
{
	QSettings settings(savePath("settings.ini"), QSettings::IniFormat, this);

	// Tags for this page
	QList<Tag> taglist;
	QStringList tagsGot;
	QStringList toAdd;
	for (int i = 0; i < m_pages.count(); i++)
	{
		QList<Tag> tags = m_pages.value(m_pages.keys().at(i))->tags();
		for (int t = 0; t < tags.count(); t++)
		{
			if (!tags[t].text().isEmpty())
			{
				if (tags[t].count() >= settings.value("tagsautoadd", 10).toInt()
					&& !m_completion.contains(tags[t].text()))
				{
					toAdd.append(tags[t].text());
					m_completion.append(tags[t].text());
				}
				if (tagsGot.contains(tags[t].text()))
				{ taglist[tagsGot.indexOf(tags[t].text())].setCount(taglist[tagsGot.indexOf(tags[t].text())].count()+tags[t].count()); }
				else
				{
					taglist.append(tags[t]);
					tagsGot.append(tags[t].text());
				}
			}
		}
	}
	QFile wordsc(savePath("wordsc.txt"));
	if (wordsc.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
	{
		wordsc.write(QString('\n').toLatin1());
		wordsc.write(toAdd.join(' ').toLatin1());
		wordsc.close();
	}

	// We sort tags by frequency
	qSort(taglist.begin(), taglist.end(), sortByFrequency);

	// Then we show them, styled if possible
	QStringList tlist = QStringList() << "artists" << "circles" << "copyrights" << "characters" << "models" << "generals" << "favorites" << "blacklisteds";
	QStringList defaults = QStringList() << "#aa0000" << "#55bbff" << "#aa00aa" << "#00aa00" << "#0000ee" << "#000000" << "#ffc0cb" << "#000000";
	QMap<QString,QString> styles;
	for (int i = 0; i < tlist.size(); i++)
	{
		QFont font;
		font.fromString(settings.value("Coloring/Fonts/"+tlist.at(i)).toString());
		styles[tlist.at(i)] = "color:"+settings.value("Coloring/Colors/"+tlist.at(i), defaults.at(i)).toString()+"; "+qfonttocss(font);
	}
	QString tags;
	for (int i = 0; i < taglist.count(); i++)
	{
		bool favorited = false;
		for (Favorite fav : m_favorites)
			if (fav.getName() == taglist[i].text())
				favorited = true;
		if (favorited)
			taglist[i].setType("favorite");
	}
	m_tags = taglist;
	m_parent->setTags(m_tags, this);

	// Wiki
	if (!page->wiki().isEmpty())
	{
		m_wiki = "<style>.title { font-weight: bold; } ul { margin-left: -30px; }</style>"+page->wiki();
		m_parent->ui->labelWiki->setText(m_wiki);
	}

	int maxpage = page->pagesCount();
	if (maxpage < m_pagemax || m_pagemax == -1)
	{ m_pagemax = maxpage; }
	ui->buttonNextPage->setEnabled(maxpage > ui->spinPage->value() || page->imagesCount() == -1 || (page->imagesCount() == 0 && page->images().count() > 0));
	ui->buttonLastPage->setEnabled(maxpage > ui->spinPage->value());
}

void tagTab::finishedLoadingPreview(Image *img)
{
	if (m_stop)
	{ return; }

	int position = m_images.indexOf(img), page = 0;
	if (!ui->checkMergeResults->isChecked())
	{
		page = m_pages.values().indexOf(img->page());
		if (page < 0)
		{ return; }
	}
	if (img->previewImage().isNull())
		return;

	QString unit;
	int size = img->fileSize();
	if (size >= 2048)
	{
		size /= 1024;
		if (size >= 2048)
		{
			size /= 1024;
			unit = "mo";
		}
		else
		{ unit = "ko"; }
	}
	else
	{ unit = "o"; }

	QSettings settings(savePath("settings.ini"), QSettings::IniFormat, this);
	QColor color;
	if (img->status() == "pending")
	{ color = QColor("#0000ff"); }
	if (img->parentId() != 0)
	{ color = QColor("#cccc00"); }
	if (img->hasChildren())
	{ color = QColor("#00ff00"); }
	for (int i = 0; i < img->tags().count(); i++)
	{
		if (!m_search->toPlainText().trimmed().split(" ").contains(img->tags()[i].text()))
		{
			for (Favorite fav : m_favorites)
			{
				if (fav.getName() == img->tags()[i].text())
				{
					color = QColor("#ffc0cb");
					break;
				}
			}
		}
	}
	QStringList blacklistedtags(settings.value("blacklistedtags").toString().split(" "));
	QStringList whitelistedtags(settings.value("whitelistedtags").toString().split(" "));
	QStringList detected = img->blacklisted(blacklistedtags);
	QStringList whitelisted = img->blacklisted(whitelistedtags);
	if (!whitelisted.isEmpty() && settings.value("whitelist_download", "image").toString() == "page")
	{
		bool download = false;
		if (!detected.isEmpty())
		{
			int reponse = QMessageBox::question(this, tr("Grabber"), tr("Certains tags de l'image sont dans la liste blanche : \"%1\". Cependant, certains dans la liste noire : \"%2\". Voulez-vous la télécharger tout de même ?").arg(whitelisted.join(", "), detected.join(", ")), QMessageBox::Yes | QMessageBox::Open | QMessageBox::No);
			if (reponse == QMessageBox::Yes)
			{ download = true; }
			else if (reponse == QMessageBox::Open)
			{
                zoomWindow *zoom = new zoomWindow(img, img->page()->site(), m_sites, this);
				zoom->show();
				connect(zoom, SIGNAL(linkClicked(QString)), this, SLOT(setTags(QString)));
				connect(zoom, SIGNAL(poolClicked(int, QString)), _mainwindow, SLOT(addPoolTab(int, QString)));
			}
		}
		else
		{ download = true; }

		if (download)
		{
			connect(img, SIGNAL(finishedImage(Image*)), _mainwindow, SLOT(saveImage(Image*)));
			connect(img, SIGNAL(finishedImage(Image*)), _mainwindow, SLOT(decreaseDownloads()));

			QString filename = settings.value("Save/filename").toString();;
			bool getTags = filename.startsWith("javascript:") || (filename.contains("%filename%") && img->site().contains("Regex/ForceImageUrl"));
			QStringList forbidden = QStringList() << "artist" << "copyright" << "character" << "model" << "general";
			for (int i = 0; i < forbidden.count(); i++)
			{ getTags = getTags || filename.contains("%"+forbidden.at(i)+"%"); }

			if (getTags)
			{
				connect(img, SIGNAL(finishedLoadingTags(Image*)), img, SLOT(loadImage()));
				img->loadDetails();
			}
			else
			{ img->loadImage(); }

			_mainwindow->increaseDownloads();
		}
	}
	if (!detected.isEmpty())
	{ color = QColor("#000000"); }
	QBouton *l = new QBouton(position, settings.value("resizeInsteadOfCropping", true).toBool(), settings.value("borders", 3).toInt(), color, this);
		l->setCheckable(true);
		l->setChecked(m_selectedImages.contains(img->url()));
		l->setToolTip(QString("%1%2%3%4%5%6%7%8")
					  .arg(img->tags().isEmpty() ? " " : tr("<b>Tags :</b> %1").arg(img->stylishedTags(m_ignored).join(" ")))
			.arg(img->id() == 0 ? " " : tr("<br/><br/><b>ID :</b> %1").arg(img->id()))
			.arg(img->rating().isEmpty() ? " " : tr("<br/><b>Classe :</b> %1").arg(img->rating()))
			.arg(img->hasScore() ? tr("<br/><b>Score :</b> %1").arg(img->score()) : " ")
			.arg(img->author().isEmpty() ? " " : tr("<br/><b>Posteur :</b> %1").arg(img->author()))
			.arg(img->width() == 0 || img->height() == 0 ? " " : tr("<br/><br/><b>Dimensions :</b> %1 x %2").arg(QString::number(img->width()), QString::number(img->height())))
			.arg(img->fileSize() == 0 ? " " : tr("<br/><b>Taille :</b> %1 %2").arg(QString::number(size), unit))
			.arg(!img->createdAt().isValid() ? " " : tr("<br/><b>Date :</b> %1").arg(img->createdAt().toString(tr("'le 'dd/MM/yyyy' à 'hh:mm"))))
		);
		l->scale(img->previewImage(), settings.value("thumbnailUpscale", 1.0f).toFloat());
		l->setFlat(true);
		connect(l, SIGNAL(appui(int)), this, SLOT(webZoom(int)));
		connect(l, SIGNAL(toggled(int,bool)), this, SLOT(toggleImage(int,bool)));
		connect(l, SIGNAL(rightClick(int)), _mainwindow, SLOT(batchChange(int)));
	int perpage = img->page()->site()->value("Urls/Selected/Tags").contains("{limit}") ? ui->spinImagesPerPage->value() : img->page()->images().size();
	perpage = perpage > 0 ? perpage : 20;
	int pp = perpage;
	if (ui->checkMergeResults->isChecked() && !m_images.empty())
	{ pp = m_images.count(); }
	int pl = ceil(sqrt((double)pp));
	if (m_layouts.size() > page)
	{ m_layouts[page]->addWidget(l, floor(float(position % pp) / pl), position % pl); }
	m_boutons.append(l);
}

void tagTab::webZoom(int id)
{
	QSettings settings(savePath("settings.ini"), QSettings::IniFormat, this);
	Image *image = m_images.at(id);

	if (!settings.value("blacklistedtags").toString().isEmpty())
	{
		QStringList blacklistedtags(settings.value("blacklistedtags").toString().split(" "));
		QStringList detected = image->blacklisted(blacklistedtags);
		if (!detected.isEmpty())
		{
			int reply = QMessageBox::question(m_parent, tr("List noire"), tr("%n tag(s) figurant dans la liste noire détécté(s) sur cette image : %1. Voulez-vous l'afficher tout de même ?", "", detected.size()).arg(detected.join(", ")), QMessageBox::Yes | QMessageBox::No);
			if (reply == QMessageBox::No)
			{ return; }
		}
	}

    zoomWindow *zoom = new zoomWindow(image, image->page()->site(), m_sites, this);
	zoom->show();
	connect(zoom, SIGNAL(linkClicked(QString)), this, SLOT(setTags(QString)));
	connect(zoom, SIGNAL(poolClicked(int, QString)), _mainwindow, SLOT(addPoolTab(int, QString)));
}

void tagTab::toggleImage(int id, bool toggle)
{
	if (toggle)
	{ selectImage(m_images.at(id)); }
	else
	{ unselectImage(m_images.at(id)); }
}

void tagTab::setTags(QString tags)
{
	activateWindow();
	m_search->setText(tags);
	load();
}

void tagTab::getPage()
{
	if (m_pages.empty())
		return;
	QStringList actuals, keys = m_sites->keys();
	for (int i = 0; i < m_checkboxes.count(); i++)
	{
		if (m_checkboxes.at(i)->isChecked())
		{ actuals.append(keys.at(i)); }
	}
	QSettings settings(savePath("settings.ini"), QSettings::IniFormat, this);
	bool unloaded = settings.value("getunloadedpages", false).toBool();
	for (int i = 0; i < actuals.count(); i++)
	{
		if (m_pages.contains(actuals.at(i)))
		{
			int perpage = unloaded ? ui->spinImagesPerPage->value() : (m_pages.value(actuals.at(i))->images().count() > ui->spinImagesPerPage->value() ? m_pages.value(actuals.at(i))->images().count() : ui->spinImagesPerPage->value());
			if (perpage <= 0 || m_pages.value(actuals.at(i))->images().count() <= 0)
				continue;

			QString search = m_pages.value(actuals.at(i))->search().join(' ');
			emit batchAddGroup(QStringList()
							   << search
							   << QString::number(ui->spinPage->value())
							   << QString::number(perpage)
							   << QString::number(perpage)
							   << settings.value("downloadblacklist").toString()
							   << actuals.at(i)
							   << settings.value("Save/filename").toString()
							   << settings.value("Save/path").toString()
							   << "");
		}
	}
}
void tagTab::getAll()
{
	if (m_pages.empty())
		return;

	QStringList actuals, keys = m_sites->keys();
	for (int i = 0; i < m_checkboxes.count(); i++)
	{
		if (m_checkboxes.at(i)->isChecked())
			actuals.append(keys.at(i));
	}

	QSettings settings(savePath("settings.ini"), QSettings::IniFormat, this);
	for (int i = 0; i < actuals.count(); i++)
	{
		int limit = m_pages.value(actuals.at(i))->highLimit();
		int v1 = qMin((limit > 0 ? limit : 200), qMax(m_pages.value(actuals.at(i))->images().count(), m_pages.value(actuals.at(i))->imagesCount()));
		int v2 = qMax(m_pages.value(actuals.at(i))->images().count(), m_pages.value(actuals.at(i))->imagesCount());
		if (v1 == 0 && v2 == 0)
			continue;

		QString search = m_pages.value(actuals.at(i))->search().join(' ');
		emit batchAddGroup(QStringList()
						   << search
						   << "1"
						   << QString::number(v1)
						   << QString::number(v2)
						   << settings.value("downloadblacklist").toString()
						   << actuals.at(i)
						   << settings.value("Save/filename").toString()
						   << settings.value("Save/path").toString()
						   << "");
	}
}
void tagTab::getSel()
{
	if (m_selectedImagesPtrs.empty())
		return;

	QSettings settings(savePath("settings.ini"), QSettings::IniFormat, this);
	for (Image *img : m_selectedImagesPtrs)
	{
		QStringList tags;
		for (Tag tag : img->tags())
			tags.append(tag.typedText());

		QMap<QString,QString> values;
		values.insert("id", QString::number(img->id()));
		values.insert("md5", img->md5());
		values.insert("rating", img->rating());
		values.insert("tags", tags.join(" "));
		values.insert("file_url", img->fileUrl().toString());
		values.insert("date", img->createdAt().toString(Qt::ISODate));
		values.insert("site", img->site());
		values.insert("filename", settings.value("Save/filename").toString());
		values.insert("folder", settings.value("Save/path").toString());

		values.insert("page_url", m_sites->value(img->site())->value("Urls/Html/Post"));
		QString t = m_sites->value(img->site())->contains("DefaultTag") ? m_sites->value(img->site())->value("DefaultTag") : "";
		values["page_url"].replace("{tags}", t);
		values["page_url"].replace("{id}", values["id"]);

		emit batchAddUnique(values);
	}

	m_selectedImagesPtrs.clear();
	m_selectedImages.clear();
	for (QBouton *l : m_boutons)
	{ l->setChecked(false); }
}

void tagTab::firstPage()
{
	ui->spinPage->setValue(1);
	load();
}
void tagTab::previousPage()
{
	if (ui->spinPage->value() > 1)
	{
		ui->spinPage->setValue(ui->spinPage->value()-1);
		load();
	}
}
void tagTab::nextPage()
{
	if (ui->spinPage->value() < ui->spinPage->maximum())
	{
		ui->spinPage->setValue(ui->spinPage->value()+1);
		load();
	}
}
void tagTab::lastPage()
{
	ui->spinPage->setValue(m_pagemax);
	load();
}



void tagTab::linkHovered(QString url)
{ m_link = url; }
void tagTab::linkClicked(QString url)
{
	if (Qt::ControlModifier)
	{ _mainwindow->addTab(url); }
	else
	{
		m_search->setPlainText(url);
		load();
	}
}
void tagTab::contextMenu()
{
	QMenu *menu = new QMenu(this);
	if (!this->m_link.isEmpty())
	{
		bool favorited = false;
		for (Favorite fav : m_favorites)
			if (fav.getName() == m_link)
				favorited = true;
		if (favorited)
		{ menu->addAction(QIcon(":/images/icons/remove.png"), tr("Retirer des favoris"), this, SLOT(unfavorite())); }
		else
		{ menu->addAction(QIcon(":/images/icons/add.png"), tr("Ajouter aux favoris"), this, SLOT(favorite())); }

		QStringList vil = loadViewItLater();
		if (vil.contains(m_link, Qt::CaseInsensitive))
		{ menu->addAction(QIcon(":/images/icons/remove.png"), tr("Ne pas garder pour plus tard"), this, SLOT(unviewitlater())); }
		else
		{ menu->addAction(QIcon(":/images/icons/add.png"), tr("Garder pour plus tard"), this, SLOT(viewitlater())); }

		menu->addSeparator();
		menu->addAction(QIcon(":/images/icons/tab-plus.png"), tr("Ouvrir dans un nouvel onglet"), this, SLOT(openInNewTab()));
		menu->addAction(QIcon(":/images/icons/window.png"), tr("Ouvrir dans une nouvelle fenêtre"), this, SLOT(openInNewWindow()));
	}
	menu->exec(QCursor::pos());
}
void tagTab::openInNewTab()
{ _mainwindow->addTab(m_link); }
void tagTab::openInNewWindow()
{
	QProcess myProcess;
	myProcess.startDetached(qApp->arguments().at(0), QStringList(m_link));
}
void tagTab::viewitlater()
{
	QStringList vil = loadViewItLater();
	vil.append(m_link);
	QFile f(savePath("viewitlater.txt"));
	f.open(QIODevice::WriteOnly);
		f.write(vil.join("\r\n").toUtf8());
	f.close();
}
void tagTab::unviewitlater()
{
	QStringList vil = loadViewItLater();
	vil.removeAll(m_link);
	QFile f(savePath("viewitlater.txt"));
	f.open(QIODevice::WriteOnly);
		f.write(vil.join("\r\n").toUtf8());
	f.close();
}

void tagTab::historyBack()
{
	if (m_history_cursor > 0)
	{
		m_from_history = true;
		m_history_cursor--;

		ui->spinPage->setValue(m_history[m_history_cursor]["page"].toInt());
		ui->spinImagesPerPage->setValue(m_history[m_history_cursor]["ipp"].toInt());
		ui->spinColumns->setValue(m_history[m_history_cursor]["columns"].toInt());
		setTags(m_history[m_history_cursor]["tags"]);

		ui->buttonHistoryNext->setEnabled(true);
		if (m_history_cursor == 0)
		{ ui->buttonHistoryBack->setEnabled(false); }
	}
}
void tagTab::historyNext()
{
	if (m_history_cursor < m_history.size() - 1)
	{
		m_from_history = true;
		m_history_cursor++;

		ui->spinPage->setValue(m_history[m_history_cursor]["page"].toInt());
		ui->spinImagesPerPage->setValue(m_history[m_history_cursor]["ipp"].toInt());
		ui->spinColumns->setValue(m_history[m_history_cursor]["columns"].toInt());
		setTags(m_history[m_history_cursor]["tags"]);

		ui->buttonHistoryBack->setEnabled(true);
		if (m_history_cursor == m_history.size() - 1)
		{ ui->buttonHistoryNext->setEnabled(false); }
	}
}

void tagTab::focusSearch()
{
	m_search->setFocus();
}

void tagTab::setImagesPerPage(int ipp)
{ ui->spinImagesPerPage->setValue(ipp); }
void tagTab::setColumns(int columns)
{ ui->spinColumns->setValue(columns); }
void tagTab::setPostFilter(QString postfilter)
{ m_postFiltering->setText(postfilter); }

int tagTab::imagesPerPage()		{ return ui->spinImagesPerPage->value();	}
int tagTab::columns()			{ return ui->spinColumns->value();			}
QString tagTab::postFilter()	{ return m_postFiltering->toPlainText();	}
QString tagTab::tags()			{ return m_search->toPlainText();			}
QString tagTab::wiki()			{ return m_wiki;							}
