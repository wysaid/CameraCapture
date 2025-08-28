//! Async support for ccap
//! 
//! This module provides async/await interfaces for camera capture operations.

#[cfg(feature = "async")]
use crate::{Provider as SyncProvider, Result, VideoFrame};
#[cfg(feature = "async")]
use std::sync::Arc;
#[cfg(feature = "async")]
use tokio::sync::{mpsc, Mutex};
#[cfg(feature = "async")]
use std::time::Duration;

#[cfg(feature = "async")]
/// Async camera provider wrapper
pub struct AsyncProvider {
    provider: Arc<Mutex<SyncProvider>>,
    frame_receiver: Option<mpsc::UnboundedReceiver<VideoFrame>>,
    _frame_sender: mpsc::UnboundedSender<VideoFrame>,
}

#[cfg(feature = "async")]
impl AsyncProvider {
    /// Create a new async provider
    pub async fn new() -> Result<Self> {
        let provider = SyncProvider::new()?;
        let (tx, rx) = mpsc::unbounded_channel();
        
        Ok(AsyncProvider {
            provider: Arc::new(Mutex::new(provider)),
            frame_receiver: Some(rx),
            _frame_sender: tx,
        })
    }

    /// Find available camera devices
    pub async fn find_device_names(&self) -> Result<Vec<String>> {
        let provider = self.provider.lock().await;
        provider.find_device_names()
    }

    /// Open a camera device
    pub async fn open(&self, device_name: Option<&str>, auto_start: bool) -> Result<()> {
        let mut provider = self.provider.lock().await;
        provider.open_device(device_name, auto_start)
    }

    /// Start capturing frames
    pub async fn start(&self) -> Result<()> {
        let mut provider = self.provider.lock().await;
        provider.start()
    }

    /// Stop capturing frames
    pub async fn stop(&self) -> Result<()> {
        let mut provider = self.provider.lock().await;
        provider.stop()
    }

    /// Check if the camera is opened
    pub async fn is_opened(&self) -> bool {
        let provider = self.provider.lock().await;
        provider.is_opened()
    }

    /// Check if capture is started
    pub async fn is_started(&self) -> bool {
        let provider = self.provider.lock().await;
        provider.is_started()
    }

    /// Grab a frame asynchronously with timeout
    pub async fn grab_frame_timeout(&self, timeout: Duration) -> Result<Option<VideoFrame>> {
        let provider = Arc::clone(&self.provider);
        let timeout_ms = timeout.as_millis() as u32;
        
        tokio::task::spawn_blocking(move || {
            let mut provider = provider.blocking_lock();
            provider.grab_frame(timeout_ms)
        }).await.map_err(|e| crate::CcapError::InternalError(e.to_string()))?
    }

    /// Grab a frame asynchronously (non-blocking)
    pub async fn grab_frame(&self) -> Result<Option<VideoFrame>> {
        self.grab_frame_timeout(Duration::from_millis(0)).await
    }

    #[cfg(feature = "async")]
    /// Create a stream of frames
    pub fn frame_stream(&mut self) -> impl futures::Stream<Item = VideoFrame> {
        let receiver = self.frame_receiver.take()
            .expect("Frame stream can only be created once");
        
        tokio_stream::wrappers::UnboundedReceiverStream::new(receiver)
    }
}

#[cfg(feature = "async")]
#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn test_async_provider_creation() {
        let provider = AsyncProvider::new().await;
        assert!(provider.is_ok());
    }

    #[tokio::test]
    async fn test_async_find_devices() {
        let provider = AsyncProvider::new().await.expect("Failed to create provider");
        let devices = provider.find_device_names().await;
        
        match devices {
            Ok(devices) => {
                println!("Found {} devices:", devices.len());
            }
            Err(e) => {
                println!("No devices found or error: {:?}", e);
            }
        }
    }
}
