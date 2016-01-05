//
//  ElectronDocument.swift
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Foundation

class ElectronDocument: MachineDocument {

	private var electron = CSElectron()
	override init() {
		super.init()
		self.intendedCyclesPerSecond = 2000000
	}

//	override func windowControllerDidLoadNib(aController: NSWindowController) {
//		super.windowControllerDidLoadNib(aController)
//	}

	override var windowNibName: String? {
		return "ElectronDocument"
	}

	override func readFromData(data: NSData, ofType typeName: String) throws {
	}

	// MARK: CSOpenGLViewDelegate
	override func runForNumberOfCycles(numberOfCycles: Int32) {
		electron.runForNumberOfCycles(numberOfCycles)
	}

	// MARK: CSOpenGLViewResponderDelegate
//	func keyDown(event: NSEvent) {}
//	func keyUp(event: NSEvent) {}
//	func flagsChanged(newModifiers: NSEvent) {}

}
