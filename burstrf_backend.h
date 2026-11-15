#ifndef BURSTRF_BACKEND_H
#define BURSTRF_BACKEND_H

#include <QObject>
#include <QImage>
#include <QUrl>
#include <QThread>
#include <QAtomicInt>
#include <QString>

#include <sigmf_io/recording.h>
#include "spectrogram_builder.h"

/*
 * * Migrate stateful values to Q_PROPERTIES to better sync frontend display.
 * * With this, we can establish an invariant for "no file" where these properties
 *   have default values, reducing chance for invalid state - default chunk_size, chunk_count, width, etc. etc.
 * * Do propper clearing of state when a file is changed/switched - clear out UI, clear out recording, etc. etc.
 *
 */

class BurstRFBackend : public QObject {
    Q_OBJECT
    // Properties bind C++ functions to QML elements, allowing them to be updated automatically when a value is changed.
    Q_PROPERTY(QString currentFileDescriptor READ currentFileDescriptor NOTIFY currentFileDescriptorChanged)
    Q_PROPERTY(double getCenterFrequencyHz READ getCenterFrequencyHz NOTIFY getCenterFrequencyHzChanged)
    Q_PROPERTY(double getSampleRateSps READ getSampleRateSps NOTIFY getSampleRateSpsChanged)
    Q_PROPERTY(int64_t spectrogramWidth READ spectrogramWidth NOTIFY spectrogramWidthChanged)
    Q_PROPERTY(int64_t spectrogramChunkHeight READ spectrogramChunkHeight NOTIFY spectrogramChunkHeightChanged)
    Q_PROPERTY(int64_t spectrogramChunkCount READ spectrogramChunkCount NOTIFY spectrogramChunkCountChanged)
    Q_PROPERTY(int64_t totalRows READ totalRows NOTIFY totalRowsChanged)
public:
    explicit BurstRFBackend(QObject* parent = nullptr);
    ~BurstRFBackend() override;

    // Loads a SigMF Metadata file, given a QUrl.
    Q_INVOKABLE bool loadFile(const QUrl &url);

    // Called by QML. Queues the work onto the worker thread and returns immediately.
    Q_INVOKABLE void generateChunk(int64_t chunk_index);

    // QML-callable function to get the current recording's file anme.
    QString currentFileDescriptor() const;

    // functions to get needed information for the Frequency Axis
    double getCenterFrequencyHz() const;

    // functions to get needed information for the Frequency Axis
    double getSampleRateSps() const;

    // so display knows how wide to set the flickable.
    int64_t spectrogramWidth() const { return this->spectrogram_builder.width(); }

    // Used by QML frontend to get the height of a single chunk of the spectrogram for chunk display.
    // TODO: Make this a Q_PROPERTY -> that way it can automatically update/adjust when the chunk height changes.
    int64_t spectrogramChunkHeight() const;

    // Used by QML frontend to know how many spectrogram chunk objects to put in the scrollable window (flickable).
    // TODO: make this a Q_PROPERTY as well...
    int64_t spectrogramChunkCount() const;

    // Used by QML to get the total number of rows for the spectrogram view.
    Q_INVOKABLE int64_t totalRows() const;

private:
    // Runs on the worker thread. Returns the chunk image (or a placeholder).
    QImage buildChunk(int64_t chunk_index) const;

    // TODO: couple with spectrogram builder so we can have a consistent size with a current recording when waiting for a
    // chunk to load. Should still allow for it to be called w/out a Recording object, as we need to set placeholders for "no file"
    // scenario.
    QImage generatePlaceholder(int chunkW, int chunkH) const;

signals:
    // Core Logic Signals - initiate data loading interactions with the UI and other components.
    void recordingLoaded();
    void chunkReady(int64_t index, const QImage &img);
    void requestChunk(int64_t index);   // internal: GUI -> worker

    // Properties the UI can read to build components properly.
    void currentFileDescriptorChanged();
    void getCenterFrequencyHzChanged();
    void getSampleRateSpsChanged();
    void spectrogramWidthChanged();
    void spectrogramChunkHeightChanged();
    void spectrogramChunkCountChanged();
    void totalRowsChanged();

private:
    int64_t chunk_size = 600'000;
    std::unique_ptr<sigmf_io::Recording> m_recording;
    burstrf::SpectrogramBuilder spectrogram_builder;

    QThread m_thread;        // the single worker thread
    QObject m_worker;        // lives on m_thread; receives requestChunk
    QAtomicInt m_pending;    // chunks queued or in progress
};

#endif // BURSTRF_BACKEND_H
