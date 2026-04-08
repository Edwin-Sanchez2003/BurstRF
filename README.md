# BurstRF - 1.0
This application is meant to be an MVP SigMF annotation editor.

## TODO
* generated half of 0.1 - need to understand properly before continuing.
* ~Add in File Select button for dataset.~
* ~libsigmf integration.~
* FFT (KissFFT?) integration.
* Add in code to load IQ data from dataset.
* Add in code to generate spectrogram images from IQ data.
* MAKE SURE TO TIE FRONTEND variables to backend rendering:
    - flickable content height - same as total spectrogram height.
    - image chunk width & height - same as spectrogram chunks.
    - resizing shouldn't mess up view (image should not be scaled ever).
    - ...others?

## 0.1 User Story
1. Open the application.
2. Click select file button - file dialog opens.
3. User selects a file (.sigmf-meta). (Check user input!)
4. Backend is cleared & re-initialized w/new dataset; Display name is updated; Spectrogram View is cleared & initial view is populated.
5. User scrolls around flickable - frontend triggers loading from backend.
6. Back to 2 or quit app.

## 1.0 Requirements

### 0.1 - Spectrogram Viewer
* Button to select a .sigmf-meta file.
* Dataset/File name displayed center/top of screen.
* Spectrogram of dataset in center view.
    * must be able to scroll up & down.
    * must be able to plot freq & time axes.

### 0.2 - Annotation Editor
* plot annotations on spectrogram (bboxes w/label).
* edit existing annotations (drag bbox corners).
* edit annotation label.
* delete annotations.
* add/create new annotations.
* save edits made overtop .sigmf-meta file.
* Undo/Redo annotation actions.

### 0.3 - Python Plugin Interface
* Un Annotation generation script on just what's in the viewport *or* the entire file.
    * This must be cancelable.
* Select script/python interpreter/Environment from a drop-down.
* Annotations must be undo/redo-able.

## Front End
* Button/File Dialog (File Select)
* Dataset Name (Display Text)
* Spectrogram Viewport (Flickable + Scrollbar)
* Annotation Add Button
* Python File Select Button
* Python Environment/Interpreter Select Button.

## Dependencies
* libsigmf (SigMF file support)
* KissFFT? (FFT)
