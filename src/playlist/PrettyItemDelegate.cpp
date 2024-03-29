#include "PrettyItemDelegate.h"
#include "../ListModel.h"
#include "../fontutils.h"
#include "../downloaditem.h"
#include "../iconloader/qticonloader.h"

#include <QFontMetricsF>
#include <QPainter>

const qreal PrettyItemDelegate::THUMB_HEIGHT = 90.0;
const qreal PrettyItemDelegate::THUMB_WIDTH = 120.0;
const qreal PrettyItemDelegate::PADDING = 10.0;

PrettyItemDelegate::PrettyItemDelegate(QObject* parent, bool downloadInfo)
    : QStyledItemDelegate(parent),
    downloadInfo(downloadInfo) {
    boldFont.setBold(true);
    smallerBoldFont = FontUtils::smallBold();
    smallerFont = FontUtils::small();

    if (downloadInfo) {
        progressBar = new QProgressBar(qApp->activeWindow());
        QPalette palette = progressBar->palette();
        palette.setColor(QPalette::Window, Qt::transparent);
        progressBar->setPalette(palette);
        progressBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        progressBar->hide();
    } else createPlayIcon();
}

void PrettyItemDelegate::createPlayIcon() {
    playIcon = QPixmap(THUMB_WIDTH, THUMB_HEIGHT);
    playIcon.fill(Qt::transparent);
    QPainter painter(&playIcon);
    QPolygon polygon;
    polygon << QPoint(PADDING*4, PADDING*2)
            << QPoint(THUMB_WIDTH-PADDING*4, THUMB_HEIGHT/2)
            << QPoint(PADDING*4, THUMB_HEIGHT-PADDING*2);
    painter.setRenderHints(QPainter::Antialiasing, true);
    painter.setBrush(Qt::white);
    QPen pen;
    pen.setColor(Qt::white);
    pen.setWidth(PADDING);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawPolygon(polygon);
}

PrettyItemDelegate::~PrettyItemDelegate() { }

QSize PrettyItemDelegate::sizeHint( const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/ ) const {
    return QSize( 256, THUMB_HEIGHT+1.0);
}

void PrettyItemDelegate::paint( QPainter* painter,
                                const QStyleOptionViewItem& option, const QModelIndex& index ) const {

    int itemType = index.data(ItemTypeRole).toInt();
    if (itemType == ItemTypeVideo) {
        QApplication::style()->drawPrimitive( QStyle::PE_PanelItemViewItem, &option, painter );
        paintBody( painter, option, index );
    } else
        QStyledItemDelegate::paint( painter, option, index );

}

void PrettyItemDelegate::paintBody( QPainter* painter,
                                    const QStyleOptionViewItem& option,
                                    const QModelIndex& index ) const {

    painter->save();
    painter->translate( option.rect.topLeft() );


    QRectF line(0, 0, option.rect.width(), option.rect.height());
    if (downloadInfo) line.setWidth(line.width() / 2);
    painter->setClipRect(line);

    const bool isActive = index.data( ActiveTrackRole ).toBool();
    const bool isSelected = option.state & QStyle::State_Selected;

    // draw the "current track" highlight underneath the text
    if (isActive && !isSelected) {
        paintActiveOverlay(painter, line.x(), line.y(), line.width(), line.height());
    }

#if defined(APP_MAC) | defined(APP_WIN)
    if (isSelected) {
        paintSelectedOverlay(painter, line.x(), line.y(), line.width(), line.height());
    }
#endif

    // get the video metadata
    const VideoPointer videoPointer = index.data( VideoRole ).value<VideoPointer>();
    const Video *video = videoPointer.data();

    // thumb
    if (!video->thumbnail().isNull()) {
        painter->drawImage(QRect(0, 0, THUMB_WIDTH, THUMB_HEIGHT), video->thumbnail());

        // play icon overlayed on the thumb
        if (isActive)
            paintPlayIcon(painter);

        // time
        QString timeString;
        int duration = video->duration();
        if ( duration > 3600 )
            timeString = QTime().addSecs(duration).toString("h:mm:ss");
        else
            timeString = QTime().addSecs(duration).toString("m:ss");
        drawTime(painter, timeString, line);

    }

    if (isActive) painter->setFont(boldFont);

    // text color
    if (isSelected)
        painter->setPen(QPen(option.palette.brush(QPalette::HighlightedText), 0));
    else
        painter->setPen(QPen(option.palette.brush(QPalette::Text), 0));

    // title
    QString videoTitle = video->title();
    QRectF textBox = line.adjusted(PADDING+THUMB_WIDTH, PADDING, -2 * PADDING, -PADDING);
    textBox = painter->boundingRect( textBox, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, videoTitle);
    painter->drawText(textBox, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, videoTitle);

    painter->setFont(smallerFont);

    // published date
    QString publishedString = video->published().date().toString(Qt::DefaultLocaleShortDate);
    QSizeF publishedStringSize(QFontMetrics(painter->font()).size( Qt::TextSingleLine, publishedString ) );
    QPointF textLoc(PADDING+THUMB_WIDTH, PADDING*2 + textBox.height());
    QRectF publishedTextBox( textLoc , publishedStringSize);
    painter->drawText(publishedTextBox, Qt::AlignLeft | Qt::AlignTop, publishedString);

    // author
    painter->save();
    painter->setFont(smallerBoldFont);
    if (!isSelected && !isActive)
        painter->setPen(QPen(option.palette.brush(QPalette::Mid), 0));
    QString authorString = video->author();
    QSizeF authorStringSize(QFontMetrics(painter->font()).size( Qt::TextSingleLine, authorString ) );
    textLoc.setX(textLoc.x() + publishedStringSize.width() + PADDING);
    QRectF authorTextBox( textLoc , authorStringSize);
    painter->drawText(authorTextBox, Qt::AlignLeft | Qt::AlignTop, authorString);
    painter->restore();

    // view count
    if (video->viewCount() >= 0) {
        painter->save();
        QLocale locale;
        QString viewCountString = tr("%1 views").arg(locale.toString(video->viewCount()));
        QSizeF viewCountStringSize(QFontMetrics(painter->font()).size( Qt::TextSingleLine, viewCountString ) );
        textLoc.setX(textLoc.x() + authorStringSize.width() + PADDING);
        QRectF viewCountTextBox( textLoc , viewCountStringSize);
        painter->drawText(viewCountTextBox, Qt::AlignLeft | Qt::AlignBottom, viewCountString);
        painter->restore();
    }

    /*
    QLinearGradient myGradient;
    QPen myPen;
    QFont myFont;
    QPointF baseline(authorTextBox.x(), authorTextBox.y() + authorTextBox.height());
    QPainterPath myPath;
    myPath.addText(baseline, boldFont, authorString);
    painter->setBrush(palette.color(QPalette::WindowText));
    painter->setPen(palette.color(QPalette::Dark));
    painter->setRenderHints (QPainter::Antialiasing, true);
    painter->drawPath(myPath);
    */

    // separator
    painter->setClipping(false);
    painter->setPen(option.palette.color(QPalette::Midlight));
    painter->drawLine(THUMB_WIDTH, THUMB_HEIGHT, option.rect.width(), THUMB_HEIGHT);
    if (!video->thumbnail().isNull())
        painter->setPen(Qt::black);
    painter->drawLine(0, THUMB_HEIGHT, THUMB_WIDTH-1, THUMB_HEIGHT);

    painter->restore();

    if (downloadInfo) paintDownloadInfo(painter, option, index);

}

void PrettyItemDelegate::paintActiveOverlay( QPainter *painter, qreal x, qreal y, qreal w, qreal h ) const {

    QPalette palette;
    QColor highlightColor = palette.color(QPalette::Highlight);
    QColor backgroundColor = palette.color(QPalette::Base);
    const float animation = 0.25;
    const int gradientRange = 16;

    QColor color2 = QColor::fromHsv(
            highlightColor.hue(),
            (int) (backgroundColor.saturation() * (1.0f - animation) + highlightColor.saturation() * animation),
            (int) (backgroundColor.value() * (1.0f - animation) + highlightColor.value() * animation)
            );
    QColor color1 = QColor::fromHsv(
            color2.hue(),
            qMax(color2.saturation() - gradientRange, 0),
            qMin(color2.value() + gradientRange, 255)
            );
    QRect rect((int) x, (int) y, (int) w, (int) h);
    painter->save();
    painter->setPen(Qt::NoPen);
    QLinearGradient linearGradient(0, 0, 0, rect.height());
    linearGradient.setColorAt(0.0, color1);
    linearGradient.setColorAt(1.0, color2);
    painter->setBrush(linearGradient);
    painter->drawRect(rect);
    painter->restore();
}

void PrettyItemDelegate::paintSelectedOverlay( QPainter *painter, qreal x, qreal y, qreal w, qreal h ) const {
    QColor color1 = QColor::fromRgb(0x69, 0xa6, 0xd9);
    QColor color2 = QColor::fromRgb(0x14, 0x6b, 0xd4);
    QRect rect((int) x, (int) y, (int) w, (int) h);
    painter->save();
    painter->setPen(Qt::NoPen);
    QLinearGradient linearGradient(0, 0, 0, rect.height());
    linearGradient.setColorAt(0.0, color1);
    linearGradient.setColorAt(1.0, color2);
    painter->setBrush(linearGradient);
    painter->drawRect(rect);
    painter->restore();
}

void PrettyItemDelegate::paintPlayIcon(QPainter *painter) const {
    painter->save();
    painter->setOpacity(.5);
    painter->drawPixmap(playIcon.rect(), playIcon);
    painter->restore();
}

void PrettyItemDelegate::drawTime(QPainter *painter, QString time, QRectF line) const {
    static const int timePadding = 4;
    QRectF textBox = painter->boundingRect(line, Qt::AlignLeft | Qt::AlignTop, time);
    // add padding
    textBox.adjust(0, 0, timePadding, 0);
    // move to bottom right corner of the thumb
    textBox.translate(THUMB_WIDTH - textBox.width(), THUMB_HEIGHT - textBox.height());

    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::black);
    painter->setOpacity(.5);
    painter->drawRect(textBox);
    painter->restore();

    painter->save();
    painter->setPen(Qt::white);
    painter->drawText(textBox, Qt::AlignCenter, time);
    painter->restore();
}

void PrettyItemDelegate::paintDownloadInfo( QPainter* painter,
                                            const QStyleOptionViewItem& option,
                                            const QModelIndex& index ) const {

    // get the video metadata
    const DownloadItemPointer downloadItemPointer = index.data(DownloadItemRole).value<DownloadItemPointer>();
    const DownloadItem *downloadItem = downloadItemPointer.data();

    painter->save();

    const QRect line(0, 0, option.rect.width() / 2, option.rect.height());

    painter->translate(option.rect.topLeft());
    painter->translate(line.width(), 0);

    QString message;
    DownloadItemStatus status = downloadItem->status();

    if (status == Downloading) {
        QString downloaded = DownloadItem::formattedFilesize(downloadItem->bytesReceived());
        QString total = DownloadItem::formattedFilesize(downloadItem->bytesTotal());
        QString speed = DownloadItem::formattedSpeed(downloadItem->currentSpeed());
        QString eta = DownloadItem::formattedTime(downloadItem->remainingTime());

        message = tr("%1 of %2 (%3) — %4").arg(
                downloaded,
                total,
                speed,
                eta
                );
    } else if (status == Starting) {
        message = tr("Preparing");
    } else if (status == Failed) {
        message = tr("Failed") + " — " + downloadItem->errorMessage();
    } else if (status == Finished) {
        message = tr("Completed");
    } else if (status == Idle) {
        message = tr("Stopped");
    }

    // progressBar->setPalette(option.palette);
    if (status == Finished) {
        progressBar->setValue(100);
        progressBar->setEnabled(true);
    } else if (status == Downloading) {
        progressBar->setValue(downloadItem->currentPercent());
        progressBar->setEnabled(true);
    } else {
        progressBar->setValue(0);
        progressBar->setEnabled(false);
    }

    int progressBarWidth = line.width() - PADDING*4 - 16;
    progressBar->setMaximumWidth(progressBarWidth);
    progressBar->setMinimumWidth(progressBarWidth);
    painter->save();
    painter->translate(PADDING, PADDING);
    progressBar->render(painter);
    painter->restore();

    bool downloadButtonHovered = false;
    bool downloadButtonPressed = false;
    const bool isHovered = index.data(HoveredItemRole).toBool();
    if (isHovered) {
        downloadButtonHovered = index.data(DownloadButtonHoveredRole).toBool();
        downloadButtonPressed = index.data(DownloadButtonPressedRole).toBool();
    }
    QIcon::Mode iconMode;
    if (downloadButtonPressed) iconMode = QIcon::Selected;
    else if (downloadButtonHovered) iconMode = QIcon::Active;
    else iconMode = QIcon::Normal;

    if (status != Finished && status != Failed && status != Idle) {
        if (downloadButtonHovered) message = tr("Stop downloading");
        painter->save();
        QIcon closeIcon = QtIconLoader::icon("window-close");
        painter->drawPixmap(downloadButtonRect(line), closeIcon.pixmap(16, 16, iconMode));
        painter->restore();
    }

    else if (status == Finished) {
        if (downloadButtonHovered)
#ifdef APP_MAC
        message = tr("Show in %1").arg("Finder");
#else
        message = tr("Open parent folder");
#endif
        painter->save();
        QIcon searchIcon = QtIconLoader::icon("system-search");
        painter->drawPixmap(downloadButtonRect(line), searchIcon.pixmap(16, 16, iconMode));
        painter->restore();
    }

    else if (status == Failed || status == Idle) {
        if (downloadButtonHovered) message = tr("Restart downloading");
        painter->save();
        QIcon searchIcon = QtIconLoader::icon("view-refresh");
        painter->drawPixmap(downloadButtonRect(line), searchIcon.pixmap(16, 16, iconMode));
        painter->restore();
    }

    QRectF textBox = line.adjusted(PADDING, PADDING*2 + progressBar->sizeHint().height(), -2 * PADDING, -PADDING);
    textBox = painter->boundingRect( textBox, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, message);
    painter->drawText(textBox, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, message);

    painter->restore();

}

QRect PrettyItemDelegate::downloadButtonRect(QRect line) const {
    return QRect(
            line.width() - PADDING*2 - 16,
            PADDING + progressBar->sizeHint().height() / 2 - 8,
            16,
            16);
}
