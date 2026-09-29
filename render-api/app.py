"""
RF ANTENNA AUTOMATION SYSTEM - OCR API
Runs on Render (Deployment Platform)

Endpoints:
- POST /extract-ocr - Extract numbers from meter images using Pytesseract
- GET /health - Health check
"""

from fastapi import FastAPI, File, UploadFile, HTTPException
from fastapi.responses import JSONResponse
from fastapi.middleware.cors import CORSMiddleware
import cv2
import numpy as np
import pytesseract
from PIL import Image
import io
import logging
import os
from typing import Optional

# ==================== SETUP ====================
app = FastAPI(title="Antenna OCR API", version="1.0.0")

# Enable CORS for all origins
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Setup logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Tesseract configuration (Render Linux environment)
# On Render, Tesseract is installed via apt-get buildpack
os.environ['TESSDATA_PREFIX'] = '/usr/share/tesseract-ocr/4.00/tessdata'

# ==================== HEALTH CHECK ====================
@app.get("/health")
async def health_check():
    """Health check endpoint"""
    return {
        "status": "healthy",
        "service": "Antenna OCR API",
        "version": "1.0.0"
    }

# ==================== OCR EXTRACTION ====================
@app.post("/extract-ocr")
async def extract_ocr(file: UploadFile = File(...)):
    """
    Extract digital reading from meter image using OCR
    
    Args:
        file: Image file (JPEG/PNG)
    
    Returns:
        JSON with extracted value
    """
    try:
        logger.info(f"Received file: {file.filename}")
        
        # Read image from upload
        contents = await file.read()
        
        # Convert bytes to OpenCV image
        nparr = np.frombuffer(contents, np.uint8)
        frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
        
        if frame is None:
            raise HTTPException(status_code=400, detail="Invalid image file")
        
        logger.info(f"Image shape: {frame.shape}")
        
        # Process image for OCR
        extracted_text = process_image_for_ocr(frame)
        
        if not extracted_text:
            raise HTTPException(status_code=400, detail="No text found in image")
        
        # Parse extracted value
        try:
            extracted_value = float(extracted_text.replace("mA", "").strip())
        except ValueError:
            # If can't convert to float, return raw text
            extracted_value = extracted_text
        
        logger.info(f"Extracted value: {extracted_value}")
        
        return JSONResponse({
            "success": True,
            "extractedValue": extracted_value,
            "rawText": extracted_text,
            "unit": "mA"
        })
    
    except HTTPException as e:
        raise e
    except Exception as e:
        logger.error(f"Error in extract_ocr: {str(e)}")
        raise HTTPException(status_code=500, detail=f"Processing error: {str(e)}")

# ==================== IMAGE PROCESSING ====================
def process_image_for_ocr(frame):
    """
    Process image to make digital segments readable
    
    Args:
        frame: OpenCV image
    
    Returns:
        Extracted text
    """
    try:
        # 1. Convert to Grayscale
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        
        # 2. Apply Gaussian Blur to reduce noise
        blurred = cv2.GaussianBlur(gray, (5, 5), 0)
        
        # 3. Apply Thresholding (Otsu's method)
        _, thresh = cv2.threshold(
            blurred, 0, 255, 
            cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU
        )
        
        # 4. Apply morphological operations
        kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))
        morph = cv2.morphologyEx(thresh, cv2.MORPH_CLOSE, kernel)
        
        # 5. Tesseract configuration
        # --psm 7: Treat as single line of text
        # whitelist: Only look for digits and decimal
        custom_config = r'--psm 7 -c tessedit_char_whitelist=0123456789.'
        
        # 6. Extract text
        text = pytesseract.image_to_string(morph, config=custom_config)
        
        # 7. Clean extracted text
        extracted_text = text.strip()
        
        logger.info(f"Raw OCR output: '{extracted_text}'")
        
        return extracted_text
    
    except Exception as e:
        logger.error(f"Error in image processing: {str(e)}")
        raise

# ==================== BATCH EXTRACTION ====================
@app.post("/extract-ocr-batch")
async def extract_ocr_batch(files: list[UploadFile] = File(...)):
    """
    Extract readings from multiple images
    
    Args:
        files: List of image files
    
    Returns:
        JSON with list of extracted values
    """
    results = []
    
    try:
        for file in files:
            contents = await file.read()
            nparr = np.frombuffer(contents, np.uint8)
            frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
            
            if frame is None:
                results.append({"filename": file.filename, "error": "Invalid image"})
                continue
            
            extracted_text = process_image_for_ocr(frame)
            
            try:
                value = float(extracted_text.replace("mA", "").strip())
            except:
                value = extracted_text
            
            results.append({
                "filename": file.filename,
                "extractedValue": value,
                "rawText": extracted_text
            })
        
        return JSONResponse({
            "success": True,
            "count": len(results),
            "results": results
        })
    
    except Exception as e:
        logger.error(f"Batch processing error: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))

# ==================== ROOT ENDPOINT ====================
@app.get("/")
async def root():
    """Root endpoint with API documentation"""
    return {
        "service": "RF Antenna OCR Extraction API",
        "version": "1.0.0",
        "endpoints": {
            "POST /extract-ocr": "Extract number from single image",
            "POST /extract-ocr-batch": "Extract numbers from multiple images",
            "GET /health": "Health check"
        },
        "usage": {
            "single_image": "curl -X POST -F 'file=@image.jpg' http://localhost:8000/extract-ocr",
            "batch": "curl -X POST -F 'files=@img1.jpg' -F 'files=@img2.jpg' http://localhost:8000/extract-ocr-batch"
        }
    }

# ==================== ERROR HANDLERS ====================
@app.exception_handler(HTTPException)
async def http_exception_handler(request, exc):
    return JSONResponse(
        status_code=exc.status_code,
        content={
            "success": False,
            "error": exc.detail
        }
    )

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(
        app, 
        host="0.0.0.0", 
        port=int(os.environ.get("PORT", 8000)),
        log_level="info"
    )