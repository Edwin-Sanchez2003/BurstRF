# BurstRF - 1.0
This application is meant to be an MVP SigMF annotation editor.

## TODO
* generated half of 0.1 - need to understand properly before continuing.
* ~Add in File Select button for dataset.~
* ~libsigmf integration.~
* ~FFT (KissFFT?) integration.~
* Implement spectrogram logic (using fake data).
    * Write function to generate fake data.
    * write function to generate spectrogram from IQ samples.
    * integrate with UI parameters.
* Add in code to load IQ data from dataset (spectrogram with real data).
* Add in code to generate spectrogram images from IQ data.
* MAKE SURE TO TIE FRONTEND variables to backend rendering:
    - flickable content height - same as total spectrogram height.
    - image chunk width & height - same as spectrogram chunks.
    - resizing shouldn't mess up view (image should not be scaled ever).
    - ...others?
    
## Setup

1. git clone https://github.com/Edwin-Sanchez2003/BurstRF.git
2. sudo dnf install nlohmann-json-devel
3. Open project in QtCreator.

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

## Final Stop Before Finishing 1.0
* Review for bugs/potential errors...

## Front End
* Button/File Dialog (File Select)
* Dataset Name (Display Text)
* Spectrogram Viewport (Flickable + Scrollbar)
    * Edit Annotation bounding box corners
    * Edit Annotation Label Text Box
* Annotation Add Button
* Python File Select Button
* Python Environment/Interpreter Select Button.

## Dependencies
* libsigmf (SigMF file support) - https://github.com/SigMF/libsigmf
    * flatbuffers - https://flatbuffers.dev/
* KissFFT (FFT) - https://github.com/mborgerding/kissfft
