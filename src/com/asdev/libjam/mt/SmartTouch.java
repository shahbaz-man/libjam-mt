package com.asdev.libjam.mt;

public class SmartTouch implements Runnable {

	public static final long LOOP_DELAY = (long)(1000.0 / 60.0), TOUCH_TIMEOUT_NS = 25 /* 1 ms in ns */ * 1000000;
	
	/**
	*	Makes sure the release function when touch expires.
	*/
	@Override
	public void run() {
		System.out.println("SMART TOUCH START");

		while(true){
			long execS = System.nanoTime();
			for(int i = 0; i < 10; i ++){
				long val = TouchHandler.syncTTGet(i);
				if(val != 0 && Math.abs(execS - val) >= TOUCH_TIMEOUT_NS){
					for(OnTouchListener o : TouchHandler.getListeners())
						o.onRelease(i);
					TouchHandler.syncTTPut(i, 0);
				}
			}
			 
			try {
				Thread.sleep(LOOP_DELAY);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}
	}

}
