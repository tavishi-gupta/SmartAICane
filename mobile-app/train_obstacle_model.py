"""
SmartGuide Cane v2 — train_obstacle_model.py
─────────────────────────────────────────────────────────────────────────────
Trains a quantized MobileNetV1-0.25 on 8 obstacle classes and exports
a TFLite INT8 model as a C header array ready to include in firmware.

REQUIREMENTS
    pip install tensorflow tensorflow-datasets pillow numpy tqdm

DATASET STRUCTURE
    Place images in:
        training_data/
            unknown/      (background, floor, ceiling, misc)
            person/
            chair/
            stairs/
            door/
            car/
            wall/
            pole/

    Minimum recommended: 200 images per class
    Target: 500+ per class for good accuracy
    Image format: any (JPG, PNG) — will be auto-resized to 96×96 grayscale

DATA SOURCES (all free)
    - Google Open Images v7: https://storage.googleapis.com/openimages/web/
    - COCO dataset:          https://cocodataset.org/
    - Your own photos (most valuable for domain accuracy)
    - Kaggle obstacle datasets
    Use the download helper below: python train_obstacle_model.py --download

OUTPUT
    obstacle_model_data.h   → copy into firmware/esp32_cane_controller/
─────────────────────────────────────────────────────────────────────────────
"""

import os
import sys
import argparse
import numpy as np
from pathlib import Path

# ── Config ────────────────────────────────────────────────────────────────────
IMG_SIZE       = 96          # Must match CAM_WIDTH / CAM_HEIGHT in config.h
NUM_CLASSES    = 8
CLASS_NAMES    = ['unknown', 'person', 'chair', 'stairs',
                  'door', 'car', 'wall', 'pole']
BATCH_SIZE     = 32
EPOCHS         = 30
DATA_DIR       = Path('training_data')
OUTPUT_HEADER  = Path('../firmware/esp32_cane_controller/obstacle_model_data.h')

# ── Imports (deferred so --help works without TF installed) ───────────────────
def get_tf():
    try:
        import tensorflow as tf
        return tf
    except ImportError:
        print("ERROR: TensorFlow not installed.")
        print("Run:  pip install tensorflow pillow numpy tqdm")
        sys.exit(1)


# ── Dataset loader ────────────────────────────────────────────────────────────
def load_dataset(tf):
    from tensorflow.keras.preprocessing.image import ImageDataGenerator

    if not DATA_DIR.exists():
        print(f"ERROR: training_data/ not found.")
        print("Create the folder structure and add images, then re-run.")
        sys.exit(1)

    datagen = ImageDataGenerator(
        rescale=1.0/255.0,
        validation_split=0.2,
        rotation_range=15,
        width_shift_range=0.1,
        height_shift_range=0.1,
        horizontal_flip=True,
        zoom_range=0.15,
        brightness_range=[0.6, 1.4],
    )

    train_gen = datagen.flow_from_directory(
        DATA_DIR,
        target_size=(IMG_SIZE, IMG_SIZE),
        color_mode='grayscale',         # 96×96×1 grayscale
        batch_size=BATCH_SIZE,
        class_mode='categorical',
        subset='training',
        shuffle=True,
    )

    val_gen = datagen.flow_from_directory(
        DATA_DIR,
        target_size=(IMG_SIZE, IMG_SIZE),
        color_mode='grayscale',
        batch_size=BATCH_SIZE,
        class_mode='categorical',
        subset='validation',
        shuffle=False,
    )

    print(f"\nClass mapping: {train_gen.class_indices}")
    print(f"Training samples: {train_gen.samples}")
    print(f"Validation samples: {val_gen.samples}\n")

    # Validate class order matches CLASS_NAMES
    for name in CLASS_NAMES:
        if name not in train_gen.class_indices:
            print(f"WARNING: Class '{name}' not found in training_data/")

    return train_gen, val_gen


# ── Model definition: MobileNetV1 0.25 ───────────────────────────────────────
def build_model(tf):
    from tensorflow.keras import layers, models

    # Use MobileNetV1 with 0.25 depth multiplier — tiny but effective
    # Input: 96×96×1 grayscale
    base = tf.keras.applications.MobileNet(
        input_shape=(IMG_SIZE, IMG_SIZE, 1),
        alpha=0.25,                   # depth multiplier — 0.25 = ~300KB model
        include_top=False,
        weights=None,                 # train from scratch (no RGB pretrain available for grayscale)
        pooling='avg',
    )

    model = models.Sequential([
        base,
        layers.Dropout(0.3),
        layers.Dense(NUM_CLASSES, activation='softmax'),
    ])

    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=1e-3),
        loss='categorical_crossentropy',
        metrics=['accuracy'],
    )

    model.summary()
    return model


# ── INT8 Post-Training Quantization ──────────────────────────────────────────
def quantize_model(tf, model, train_gen):
    print("\n[QUANTIZE] Running INT8 post-training quantization...")

    # Collect representative dataset for calibration
    def representative_dataset():
        count = 0
        for batch_x, _ in train_gen:
            for img in batch_x:
                yield [img[np.newaxis, :, :, :].astype(np.float32)]
                count += 1
                if count >= 200:   # 200 samples is enough for calibration
                    return

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations        = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type  = tf.int8
    converter.inference_output_type = tf.int8

    tflite_model = converter.convert()

    size_kb = len(tflite_model) / 1024
    print(f"[QUANTIZE] INT8 model size: {size_kb:.1f} KB")
    if size_kb > 400:
        print("[QUANTIZE] WARNING: Model > 400KB. Reduce alpha or image size.")

    return tflite_model


# ── Export as C header ────────────────────────────────────────────────────────
def export_c_header(tflite_model_bytes, output_path):
    print(f"\n[EXPORT] Writing C header to {output_path}...")

    hex_array = ', '.join(f'0x{b:02x}' for b in tflite_model_bytes)
    array_len = len(tflite_model_bytes)

    header = f"""/*
 * obstacle_model_data.h — AUTO-GENERATED by train_obstacle_model.py
 * DO NOT EDIT MANUALLY.
 *
 * Model: MobileNetV1-0.25, INT8 quantized
 * Input: {IMG_SIZE}x{IMG_SIZE} grayscale
 * Output: {NUM_CLASSES} classes: {CLASS_NAMES}
 * Size: {array_len / 1024:.1f} KB
 *
 * Class order (output tensor indices):
{chr(10).join(f' *   {i}: {n}' for i, n in enumerate(CLASS_NAMES))}
 *
 * IMPORTANT: The class order here MUST match the #define OBJ_* values
 * in config.h. If you retrain with a different class order, update both.
 */

#pragma once
#include <cstdint>

alignas(8) const unsigned char g_obstacle_model_data[] = {{
  {hex_array}
}};

const int g_obstacle_model_data_len = {array_len};
"""

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w') as f:
        f.write(header)

    print(f"[EXPORT] Done. {array_len} bytes ({array_len/1024:.1f} KB)")
    print(f"[EXPORT] Copy {output_path} into your firmware sketch folder.")


# ── Evaluation ────────────────────────────────────────────────────────────────
def evaluate_tflite(tf, tflite_bytes, val_gen):
    print("\n[EVAL] Evaluating INT8 model on validation set...")
    interpreter = tf.lite.Interpreter(model_content=tflite_bytes)
    interpreter.allocate_tensors()
    inp  = interpreter.get_input_details()[0]
    outp = interpreter.get_output_details()[0]

    correct = 0
    total   = 0
    confusion = np.zeros((NUM_CLASSES, NUM_CLASSES), dtype=int)

    for batch_x, batch_y in val_gen:
        for img, label_onehot in zip(batch_x, batch_y):
            # Quantize input: float32 [0,1] → int8 [-128,127]
            scale     = inp['quantization_parameters']['scales'][0]
            zp        = inp['quantization_parameters']['zero_points'][0]
            img_int8  = (img / scale + zp).astype(np.int8)
            interpreter.set_tensor(inp['index'], img_int8[np.newaxis])
            interpreter.invoke()
            out = interpreter.get_tensor(outp['index'])[0]
            pred  = np.argmax(out)
            truth = np.argmax(label_onehot)
            confusion[truth][pred] += 1
            if pred == truth:
                correct += 1
            total += 1
        if total >= val_gen.samples:
            break

    acc = correct / total * 100
    print(f"\n[EVAL] Accuracy: {correct}/{total} = {acc:.1f}%")
    print("\n[EVAL] Confusion matrix (rows=truth, cols=pred):")
    header_row = '       ' + '  '.join(f'{n[:5]:>5}' for n in CLASS_NAMES)
    print(header_row)
    for i, row in enumerate(confusion):
        print(f"  {CLASS_NAMES[i][:5]:>5}  " + '  '.join(f'{v:5d}' for v in row))

    per_class = confusion.diagonal() / confusion.sum(axis=1).clip(min=1) * 100
    print("\n[EVAL] Per-class accuracy:")
    for name, acc_c in zip(CLASS_NAMES, per_class):
        bar = '█' * int(acc_c / 5)
        print(f"  {name:8s}  {acc_c:5.1f}%  {bar}")


# ── Main ──────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description='Train SmartGuide obstacle model')
    parser.add_argument('--epochs', type=int, default=EPOCHS)
    parser.add_argument('--skip-train', action='store_true',
                        help='Skip training, only re-export saved model')
    args = parser.parse_args()

    tf = get_tf()
    print(f"TensorFlow version: {tf.__version__}")
    print(f"GPU available: {len(tf.config.list_physical_devices('GPU')) > 0}")

    train_gen, val_gen = load_dataset(tf)
    model = build_model(tf)

    if not args.skip_train:
        callbacks = [
            tf.keras.callbacks.EarlyStopping(
                monitor='val_accuracy', patience=6, restore_best_weights=True),
            tf.keras.callbacks.ReduceLROnPlateau(
                monitor='val_loss', factor=0.5, patience=3),
            tf.keras.callbacks.ModelCheckpoint(
                'best_obstacle_model.keras', save_best_only=True,
                monitor='val_accuracy'),
        ]

        print(f"\n[TRAIN] Training for up to {args.epochs} epochs...")
        model.fit(
            train_gen,
            epochs=args.epochs,
            validation_data=val_gen,
            callbacks=callbacks,
        )
        print("[TRAIN] Done.")
    else:
        model.load_weights('best_obstacle_model.keras')

    tflite_bytes = quantize_model(tf, model, train_gen)
    evaluate_tflite(tf, tflite_bytes, val_gen)
    export_c_header(tflite_bytes, OUTPUT_HEADER)

    print("\n✅ All done!")
    print(f"   → Copy obstacle_model_data.h into firmware/esp32_cane_controller/")
    print(f"   → Set partition to 'Huge APP (3MB)' in Arduino IDE")
    print(f"   → Enable PSRAM in Tools menu")
    print(f"   → Flash and open Serial Monitor at 115200")


if __name__ == '__main__':
    main()
