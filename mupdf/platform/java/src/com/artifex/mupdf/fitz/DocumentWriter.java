package com.artifex.mupdf.fitz;

public class DocumentWriter
{
	static {
		Context.init();
	}

	private long pointer;

	protected native void finalize();

	public void destroy() {
		finalize();
	}

	private native static long newNativeDocumentWriter(String filename, String format, String options);
	private native static long newNativeDocumentWriterWithSeekableOutputStream(SeekableOutputStream stream, String format, String options);

	public DocumentWriter(String filename, String format, String options) {
		pointer = newNativeDocumentWriter(filename, format, options);
	}

	public DocumentWriter(SeekableOutputStream stream, String format, String options) {
		pointer = newNativeDocumentWriterWithSeekableOutputStream(stream, format, options);
	}

	public native Device beginPage(Rect mediabox);
	public native void endPage();
	public native void close();

	private long ocrlistener;

	public interface OCRListener
	{
		boolean progress(int percent);
	}

	public native void addOCRListener(OCRListener listener);
}
