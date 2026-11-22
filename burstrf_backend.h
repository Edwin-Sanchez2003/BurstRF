#ifndef BURSTRF_BACKEND_H
#define BURSTRF_BACKEND_H

#include <QObject>
#include <QImage>
#include <QUrl>
#include <QThread>
#include <QAtomicInt>
#include <QString>
#include <memory>

#include <sigmf_io/recording.h>
#include "spectrogram_builder.h"

/*
 * BurstRFBackend:
 * This class has 3 main purposes:
 * 1. Act as the interface between the backend logic for reading & processing data and the UI visualizations of that data.
 * 2. Manage/orchestrate business logic of the lower level components (ie. lifecycle management; also includes maintaining invariants
 *    for the application while its running).
 * 3. Manage threading logic to keep the GUI thread lightweight and keep UI components up-to-date at a reasonable rate.
 *      - NOTE: be wary of race conditions & logic w/respect to the pointers this class owns/handles... (recordings, threads, etc.)
 * TODO:
 * * Establish invariants for BurstRFbackend for UI stability.
 * * Do proper clearing of state when a file is changed/switched - clear out UI, clear out recording, etc. etc.
 * * Update logic to remove queued chunks that have not yet been rendered and have already left the viewable range.
 */

class BurstRFBackend : public QObject {
    Q_OBJECT
    // Properties bind C++ functions to QML elements, allowing them to be updated automatically when a value is changed.
    Q_PROPERTY(QString currentFileDescriptor READ currentFileDescriptor NOTIFY currentFileDescriptorChanged)
    Q_PROPERTY(double getCenterFrequencyHz READ getCenterFrequencyHz NOTIFY getCenterFrequencyHzChanged)
    Q_PROPERTY(double getSampleRateSps READ getSampleRateSps NOTIFY getSampleRateSpsChanged)
    Q_PROPERTY(size_t spectrogramChunkWidth READ spectrogramChunkWidth NOTIFY spectrogramChunkWidthChanged)
    Q_PROPERTY(size_t spectrogramChunkHeight READ spectrogramChunkHeight NOTIFY spectrogramChunkHeightChanged)
    Q_PROPERTY(size_t spectrogramChunkCount READ spectrogramChunkCount NOTIFY spectrogramChunkCountChanged)
    Q_PROPERTY(size_t spectrogramRowCount READ spectrogramRowCount NOTIFY spectrogramRowCountChanged)
public:
    explicit BurstRFBackend(QObject* parent = nullptr);
    ~BurstRFBackend() override;

    // Loads a SigMF Metadata file, given a QUrl.
    Q_INVOKABLE bool loadFile(const QUrl &url);

    // Called by QML. Queues the work onto the worker thread and returns immediately.
    Q_INVOKABLE void generateChunk(int64_t chunk_index);

    // QML-callable function to get the current recording's file name & other details.
    QString currentFileDescriptor() const;

    // functions to get needed information for the Frequency Axis
    double getCenterFrequencyHz() const;

    // functions to get needed information for the Frequency Axis
    double getSampleRateSps() const;

    // so display knows how wide to set the flickable.
    size_t spectrogramChunkWidth() const { return this->spectrogram_builder.params().spectrogram_chunk_width(); }

    // Used by QML frontend to get the height of a single chunk of the spectrogram for chunk display.
    // TODO: Make this a Q_PROPERTY -> that way it can automatically update/adjust when the chunk height changes.
    size_t spectrogramChunkHeight() const;

    // Used by QML frontend to know how many spectrogram chunk objects to put in the scrollable window (flickable).
    // TODO: make this a Q_PROPERTY as well...
    size_t spectrogramChunkCount() const;

    // Used by QML to get the total number of rows for the spectrogram view.
    size_t spectrogramRowCount() const;

signals:
    // Core Logic Signals - initiate data loading interactions with the UI and other components.
    void recordingLoaded();
    void chunkReady(int64_t index, const QImage &img);
    void requestChunk(int64_t index);   // internal: GUI -> worker

    // Properties the UI can read to build components properly.
    void currentFileDescriptorChanged();
    void getCenterFrequencyHzChanged();
    void getSampleRateSpsChanged();
    void spectrogramChunkWidthChanged();
    void spectrogramChunkHeightChanged();
    void spectrogramChunkCountChanged();
    void spectrogramRowCountChanged();

private:
    std::shared_ptr<sigmf_io::Recording> m_recording; // NOTE: shared_ptr -> need to keep track of who keeps it alive!!!
    burstrf::SpectrogramBuilder spectrogram_builder;

    QThread m_thread;        // the single worker thread
    QObject m_worker;        // lives on m_thread; receives requestChunk
    QAtomicInt m_pending;    // chunks queued or in progress
};

#endif // BURSTRF_BACKEND_H
